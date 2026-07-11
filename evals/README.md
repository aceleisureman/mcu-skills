# 路由评测数据集

`routing-cases.json` 是路由质量的持续评测集，由 `tools/evaluate_routing.py` 执行，
CI 与 `tools/validate.py` 会校验评测结果与 `docs/routing-evaluation.json` 报告的漂移。

## 用例格式

```json
{
  "id": "sensor-ds18b20",
  "query": "DS18B20 读不到温度怎么排查",
  "expected_skills": ["mcu-sensors"],
  "expected_targets": ["skill://mcu-sensors/references/temperature.md"]
}
```

| 字段 | 说明 |
|------|------|
| `id` | 唯一 kebab-case 标识 |
| `query` | 模拟用户提问（建议覆盖口语化、中英混合、带连字符型号等真实形态） |
| `expected_skills` | 期望被路由到的 Skill 集合；空数组表示负样本（不应命中任何 Skill） |
| `expected_targets` | 期望加载的资源 `skill://` URI 集合；负样本同样留空 |

## 负样本

与 MCU/项目整理无关的问题应写成负样本，防止关键词弱命中造成误召回：

```json
{
  "id": "negative-frontend",
  "query": "React 前端页面白屏了怎么排查",
  "expected_skills": [],
  "expected_targets": []
}
```

## 质量阈值

`evaluate_routing.py` 的通过标准（低于阈值时 validate/CI 失败）：

| 指标 | 阈值 |
|------|------|
| Skill precision | ≥ 95% |
| Skill recall | ≥ 95% |
| top-1 准确率 | ≥ 95% |
| 目标资源 recall | ≥ 90% |

## 维护流程

```sh
python3 tools/evaluate_routing.py                 # 查看当前指标
python3 tools/evaluate_routing.py --write-report  # 更新 docs/routing-evaluation.json
python3 tools/validate.py                         # 全量校验
```

新增路由或关键词后必须补充对应用例；新增用例后运行 `--write-report` 同步报告。
