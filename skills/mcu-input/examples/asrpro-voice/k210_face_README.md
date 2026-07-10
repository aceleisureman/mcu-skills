# K210 人脸识别模块

本目录代码适配 MaixPy/CanMV v1 风格 K210。STM32 只通过串口发命令，K210 本地完成摄像头采集、人脸特征提取、保存和比对。

## 文件

- `main.py`：K210 主程序，串口协议 + 摄像头 + KPU 人脸识别
- `model.py`：人脸特征文件存储和匹配

## 串口协议

STM32 -> K210：

```text
0xAA cmd param checksum 0x55
checksum = cmd ^ param
```

K210 -> STM32：

```text
0xAA status face_id confidence 0x00 0x55
```

多发一个 `0x00` 是为了匹配 STM32 当前 `K10_WaitResponse()` 等 6 字节包的逻辑。

## 模型文件

代码优先从 SD 卡加载模型，没有 SD 卡则从 Flash 地址加载。

SD 卡路径：

```text
/sd/face_detect.kmodel
/sd/face_landmark.kmodel
/sd/face_feature.kmodel
```

Flash 地址：

```text
0x300000 人脸检测模型
0x400000 关键点模型
0x500000 特征提取模型
```

模型需要使用 YAHBOOM 资料包或 Sipeed MaixPy-v1 示例的人脸识别三模型。

## 人脸数据保存

人脸特征保存到 K210 文件系统：

```text
/flash/face_01.json
/flash/face_02.json
...
```

最多 20 张。

## 上传运行

```bash
./ampy put main.py /flash/main.py
./ampy put model.py /flash/model.py
./ampy run main.py
```

如果要脱机运行，把 `main.py` 和 `model.py` 放到 K210 `/flash` 或 SD 卡，并让固件启动时执行 `main.py`。
