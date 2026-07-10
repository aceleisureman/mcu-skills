"""
K210 人脸特征存储与匹配 (CanMV 亚博 v2.1.1)

特征数据保存在 K210 文件系统 /flash/face_XX.json, STM32 不保存人脸数据。
特征比对用 CanMV 的 kpu.feature_compare (替代旧版 face_compare)。
"""
import os

try:
    import ujson as json
except ImportError:
    import json

MAX_FACES = 20
MATCH_THRESHOLD = 80
FEATURE_DIR = "/flash"

# 比对用的 kpu 实例, 由 main.py 注入 (set_kpu)
_kpu = None


def set_kpu(kpu):
    global _kpu
    _kpu = kpu


def _feature_path(face_id):
    return "%s/face_%02d.json" % (FEATURE_DIR, face_id)


def _safe_remove(path):
    try:
        os.remove(path)
    except OSError:
        pass


def save_feature(face_id, feature):
    data = []
    for v in feature:
        data.append(float(v))

    path = _feature_path(face_id)
    with open(path, "w") as f:
        f.write(json.dumps(data))
    print("[DB] saved face %d -> %s" % (face_id, path))


def load_feature(face_id):
    path = _feature_path(face_id)
    try:
        with open(path, "r") as f:
            data = json.loads(f.read())
        return data
    except Exception:
        return None


def load_all_features():
    face_db = {}
    for face_id in range(1, MAX_FACES + 1):
        feature = load_feature(face_id)
        if feature:
            face_db[face_id] = feature
    return face_db


def delete_feature(face_id):
    _safe_remove(_feature_path(face_id))
    print("[DB] deleted face %d" % face_id)


def clear_all_features():
    for face_id in range(1, MAX_FACES + 1):
        _safe_remove(_feature_path(face_id))
    print("[DB] cleared all faces")


def find_best_match(feature, face_db, threshold=MATCH_THRESHOLD):
    """遍历人脸库比对, 返回 (face_id, score)。

    feature_compare 返回相似度分数 (越高越像)。
    CanMV 官方示例阈值 80.5。
    """
    best_id = 0
    best_score = 0

    for face_id, stored_feature in face_db.items():
        try:
            score = _kpu.feature_compare(stored_feature, feature)
        except Exception:
            score = 0

        if score > best_score:
            best_score = score
            best_id = face_id

    if best_score >= threshold:
        return best_id, int(best_score)
    return 0, int(best_score)
