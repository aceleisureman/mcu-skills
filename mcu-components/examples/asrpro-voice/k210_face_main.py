#!/usr/bin/env python3
"""
K210 / CanMV (亚博 v2.1.1) 人脸录入识别模块

STM32 串口协议兼容 Hardware/K10/k10.c:
  RX: 0xAA cmd param checksum 0x55
  TX: 0xAA status face_id confidence 0x00 0x55

cmd:
  0x01 录入人脸 param=face_id
  0x02 识别人脸 param=0
  0x03 删除人脸 param=face_id
  0x04 清空人脸 param=0

KPU 用 CanMV 99-KPU 示例 API (load_kmodel / run_with_output /
regionlayer_yolo2 / feature_compare), 模型放 SD 卡 /sd/KPU/
"""

import gc
import os
import sys
import time

import image
import lcd
from maix import KPU
import sensor
from machine import UART
from fpioa_manager import fm

import model

UART_BAUD = 115200
UART_TX_PIN = 9
UART_RX_PIN = 8

CMD_FACE_ENROLL = 0x01
CMD_FACE_RECOGNIZE = 0x02
CMD_FACE_DELETE = 0x03
CMD_FACE_CLEAR = 0x04

STATUS_OK = 0x00
STATUS_FAIL = 0x01
STATUS_NO_FACE = 0x02
STATUS_TIMEOUT = 0x03
STATUS_BUSY = 0x04

# 模型路径 (CanMV 官方 99-KPU 示例布局)
MODEL_DETECT = "/sd/KPU/yolo_face_detect/face_detect_320x240.kmodel"
MODEL_LANDMARK = "/sd/KPU/face_recognization/ld5.kmodel"
MODEL_FEATURE = "/sd/KPU/face_recognization/feature_extraction.kmodel"

# 18 个 anchor (anchor_num=9), 与官方 face_recog 示例一致
ANCHORS = (0.1075, 0.126875, 0.126875, 0.175, 0.1465625, 0.2246875,
           0.1953125, 0.25375, 0.2440625, 0.351875, 0.341875, 0.4721875,
           0.5078125, 0.6696875, 0.8984375, 1.099687, 2.129062, 2.425937)

# 标准人脸 5 点 (按 64x64 缩放, FACE_PIC_SIZE=64)
FACE_PIC_SIZE = 64
DST_POINT = [(int(38.2946 * FACE_PIC_SIZE / 112), int(51.6963 * FACE_PIC_SIZE / 112)),
             (int(73.5318 * FACE_PIC_SIZE / 112), int(51.5014 * FACE_PIC_SIZE / 112)),
             (int(56.0252 * FACE_PIC_SIZE / 112), int(71.7366 * FACE_PIC_SIZE / 112)),
             (int(41.5493 * FACE_PIC_SIZE / 112), int(92.3655 * FACE_PIC_SIZE / 112)),
             (int(70.7299 * FACE_PIC_SIZE / 112), int(92.2041 * FACE_PIC_SIZE / 112))]

THRESHOLD = 80.5

uart = None
kpu = None
ld5_kpu = None
fea_kpu = None
feature_img = None
models_ready = False
face_db = {}
next_face_id = 1


def ticks_ms():
    return time.ticks_ms()


def ticks_diff(now, start):
    return time.ticks_diff(now, start)


def file_exists(path):
    try:
        os.stat(path)
        return True
    except OSError:
        return False


def camera_init():
    lcd.init()
    # gc2145 单缓冲会偶发性冻帧 (snapshot 卡死 -> 黑屏)。
    # 双缓冲可消除冻帧; 个别固件不支持时回退到单缓冲。
    try:
        sensor.reset(dual_buff=True)
        print("[CAM] dual_buff on")
    except Exception as e:
        sensor.reset()
        print("[CAM] dual_buff FAILED -> single")
        sys.print_exception(e)
    sensor.set_pixformat(sensor.RGB565)
    sensor.set_framesize(sensor.QVGA)
    sensor.set_hmirror(1)
    sensor.set_vflip(1)
    sensor.run(1)
    sensor.skip_frames(time=500)
    time.sleep_ms(300)


def models_init():
    global kpu, ld5_kpu, fea_kpu, feature_img, models_ready
    try:
        kpu = KPU()
        kpu.load_kmodel(MODEL_DETECT)
        kpu.init_yolo2(ANCHORS, anchor_num=9, img_w=320, img_h=240,
                       net_w=320, net_h=240, layer_w=10, layer_h=8,
                       threshold=0.35, nms_value=0.2, classes=1)

        ld5_kpu = KPU()
        ld5_kpu.load_kmodel(MODEL_LANDMARK)

        fea_kpu = KPU()
        fea_kpu.load_kmodel(MODEL_FEATURE)

        feature_img = image.Image(size=(FACE_PIC_SIZE, FACE_PIC_SIZE), copy_to_fb=False)
        feature_img.pix_to_ai()

        models_ready = True
        # 注入比对用的 kpu 实例 (feature_compare 在检测 kpu 上调用)
        model.set_kpu(kpu)
        print("[MODEL] ready free=%d" % gc.mem_free())
    except Exception as e:
        models_ready = False
        print("[MODEL] load failed")
        sys.print_exception(e)


def uart_init():
    global uart
    try:
        fm.register(UART_TX_PIN, fm.fpioa.UART1_TX, force=True)
        fm.register(UART_RX_PIN, fm.fpioa.UART1_RX, force=True)
        uart = UART(UART.UART1, UART_BAUD, 8, 0, 1, timeout=100, read_buf_len=64)
    except Exception:
        uart = UART(1, baudrate=UART_BAUD, timeout=100, timeout_char=50)
    print("[UART] ready")


def send_response(status, face_id=0, confidence=0):
    if confidence < 0:
        confidence = 0
    if confidence > 255:
        confidence = 255
    pkt = bytes([0xAA, status & 0xFF, face_id & 0xFF, confidence & 0xFF, 0x00, 0x55])
    uart.write(pkt)
    print("[TX] status=%d id=%d conf=%d" % (status, face_id, confidence))


def wait_command(timeout_ms=50):
    buf = bytearray()
    start = ticks_ms()
    in_packet = False

    while ticks_diff(ticks_ms(), start) < timeout_ms:
        if uart.any():
            data = uart.read(1)
            if not data:
                continue
            b = data[0]

            if not in_packet:
                if b == 0xAA:
                    buf = bytearray([b])
                    in_packet = True
                continue

            buf.append(b)
            if len(buf) == 5:
                in_packet = False
                if buf[4] == 0x55 and ((buf[1] ^ buf[2]) & 0xFF) == buf[3]:
                    return buf[1], buf[2]
                buf = bytearray()
        time.sleep_ms(1)

    return None, None


def extend_box(x, y, w, h, scale=0):
    x1_t = x - scale * w
    x2_t = x + w + scale * w
    y1_t = y - scale * h
    y2_t = y + h + scale * h
    x1 = int(x1_t) if x1_t > 1 else 1
    x2 = int(x2_t) if x2_t < 320 else 319
    y1 = int(y1_t) if y1_t > 1 else 1
    y2 = int(y2_t) if y2_t < 240 else 239
    return x1, y1, x2 - x1 + 1, y2 - y1 + 1


def extract_face_feature(img):
    """检测人脸 -> 关键点 -> 仿射对齐 -> 提取特征。返回 (status, feature)。

    KPU/图像处理任何异常都在这里吞掉, 当作本帧处理失败 (STATUS_FAIL),
    绝不让异常冒出去把主循环/整个程序搞崩导致黑屏。"""
    try:
        kpu.run_with_output(img)
        dect = kpu.regionlayer_yolo2()
        if not dect:
            return STATUS_NO_FACE, None

        l = dect[0]
        x1, y1, cut_w, cut_h = extend_box(l[0], l[1], l[2], l[3])

        face_cut = img.cut(x1, y1, cut_w, cut_h)
        face_cut_128 = face_cut.resize(128, 128)
        face_cut_128.pix_to_ai()

        out = ld5_kpu.run_with_output(face_cut_128, getlist=True)
        face_key_point = []
        for j in range(5):
            px = int(KPU.sigmoid(out[2 * j]) * cut_w + x1)
            py = int(KPU.sigmoid(out[2 * j + 1]) * cut_h + y1)
            face_key_point.append((px, py))

        transform = image.get_affine_transform(face_key_point, DST_POINT)
        image.warp_affine_ai(img, feature_img, transform)

        feature = fea_kpu.run_with_output(feature_img, get_feature=True)

        img.draw_rectangle(x1, y1, cut_w, cut_h, color=(0, 255, 0), thickness=2)

        del face_cut
        del face_cut_128
        del face_key_point
        gc.collect()
        return STATUS_OK, feature
    except Exception as e:
        print("[EXTRACT] frame failed")
        sys.print_exception(e)
        gc.collect()
        return STATUS_FAIL, None


def draw_status(img, text, color=(255, 0, 0)):
    try:
        img.draw_string(4, 4, text, color=color, scale=2)
    except Exception:
        pass


def capture_feature(timeout_ms, label):
    start = ticks_ms()
    while ticks_diff(ticks_ms(), start) < timeout_ms:
        try:
            img = sensor.snapshot()
            status, feature = extract_face_feature(img)
            if status == STATUS_OK:
                draw_status(img, "%s OK" % label, color=(0, 255, 0))
                lcd.display(img)
                print("[CAP] face captured")
                return status, feature
            # NO_FACE / FAIL 都只是本帧没认出, 继续刷屏重试下一帧
            draw_status(img, label)
            lcd.display(img)
        except Exception as e:
            print("[CAP] frame error")
            sys.print_exception(e)
            gc.collect()
        time.sleep_ms(20)
    print("[CAP] no face timeout")
    return STATUS_NO_FACE, None


def face_enroll(face_id):
    print("[ENROLL] id=%d" % face_id)
    if face_id < 1 or face_id > model.MAX_FACES:
        return STATUS_FAIL
    if not models_ready:
        print("[ENROLL] model missing")
        return STATUS_FAIL

    status, feature = capture_feature(10000, "ENROLL")
    if status != STATUS_OK:
        return status

    model.save_feature(face_id, feature)
    face_db[face_id] = list(feature)
    return STATUS_OK


def face_recognize():
    print("[RECOG]")
    if not models_ready:
        print("[RECOG] model missing")
        return STATUS_FAIL, 0, 0
    if not face_db:
        return STATUS_FAIL, 0, 0

    status, feature = capture_feature(8000, "RECOG")
    if status != STATUS_OK:
        return status, 0, 0

    face_id, score = model.find_best_match(feature, face_db)
    if face_id:
        return STATUS_OK, face_id, score
    return STATUS_NO_FACE, 0, score


def face_delete(face_id):
    if face_id in face_db:
        del face_db[face_id]
    model.delete_feature(face_id)
    return STATUS_OK


def face_clear_all():
    global next_face_id
    face_db.clear()
    model.clear_all_features()
    next_face_id = 1
    return STATUS_OK


def handle_command(cmd, param):
    global next_face_id

    print("[RX] cmd=0x%02X param=%d" % (cmd, param))

    if cmd == CMD_FACE_ENROLL:
        status = face_enroll(param)
        if status == STATUS_OK and param >= next_face_id:
            next_face_id = param + 1
        send_response(status, param, 0)
    elif cmd == CMD_FACE_RECOGNIZE:
        status, face_id, confidence = face_recognize()
        send_response(status, face_id, confidence)
    elif cmd == CMD_FACE_DELETE:
        status = face_delete(param)
        send_response(status, param, 0)
    elif cmd == CMD_FACE_CLEAR:
        status = face_clear_all()
        send_response(status, 0, 0)
    else:
        send_response(STATUS_FAIL, 0, 0)


def main():
    global face_db, next_face_id
    print("=== K210 face module (CanMV) ===")
    uart_init()
    camera_init()
    models_init()

    face_db = model.load_all_features()
    if face_db:
        next_face_id = max(face_db.keys()) + 1
    print("[DB] loaded=%d next=%d" % (len(face_db), next_face_id))

    send_response(STATUS_OK, 0, 0)
    idle_err = 0
    idle_n = 0
    while True:
        cmd, param = wait_command(10)
        if cmd is not None:
            # 命令处理任何异常只回一个 FAIL, 绝不退出 while True (否则会黑屏)
            try:
                handle_command(cmd, param)
            except Exception as e:
                print("[CMD] handler error")
                sys.print_exception(e)
                gc.collect()
                try:
                    send_response(STATUS_FAIL, 0, 0)
                except Exception:
                    pass
        else:
            try:
                img = sensor.snapshot()
                lcd.display(img)
                idle_n += 1
                # 周期性回收, 防止空闲预览长跑内存压力
                if idle_n % 60 == 0:
                    gc.collect()
            except Exception as e:
                idle_err += 1
                if idle_err <= 3:
                    print("[IDLE] display err")
                    sys.print_exception(e)
                gc.collect()


if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        sys.print_exception(e)
        try:
            send_response(STATUS_FAIL, 0, 0)
        except Exception:
            pass
    finally:
        if fea_kpu:
            fea_kpu.deinit()
        if ld5_kpu:
            ld5_kpu.deinit()
        if kpu:
            kpu.deinit()
