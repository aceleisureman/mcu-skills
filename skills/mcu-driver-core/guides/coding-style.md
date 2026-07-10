# 嵌入式 C 代码风格与版本规范

公司驱动/固件代码的统一风格约定，用于代码评审与新代码生成。

## 1. 命名规范

| 对象 | 规则 | 示例 |
|------|------|------|
| 文件 | 小写 + 下划线，驱动为 `器件名.c/.h` | `sht30.c`, `w25qxx.h` |
| 函数 | `模块_动作` 小写下划线 | `sht30_read()`, `uart_write()` |
| 类型 | 小写 + `_t` 后缀 | `sht30_handle_t`, `i2c_status_t` |
| 枚举值/宏 | 全大写 + 模块前缀 | `SHT30_OK`, `W25Q_PAGE_SIZE` |
| 全局变量 | `g_` 前缀 | `g_sys_config` |
| 静态变量 | `s_` 前缀 | `s_rx_buf` |
| 中断函数 | 平台命名不改，逻辑外提 | ISR 内只调 `xxx_irq_handler()` |

## 2. 代码结构约定

- 驱动三层结构：`器件驱动(纯逻辑) → HAL 接口(函数指针注入) → 平台适配`；驱动本体禁止包含平台头文件
- 头文件必须有 include guard 与 `extern "C"`
- 单函数 ≤ 60 行；圈复杂度过高时拆分
- 禁止动态内存（malloc）；缓冲区由调用方传入或静态分配
- 所有 API 返回统一错误码枚举；调用方必须检查返回值
- 魔法数字一律 `#define`/`enum` 命名
- 中断服务函数：只置标志/发通知，禁止总线操作、printf、长循环

## 3. 格式约定（.clang-format 基线）

```yaml
BasedOnStyle: LLVM
IndentWidth: 4
ColumnLimit: 100
PointerAlignment: Right       # int *p
BreakBeforeBraces: Linux      # 函数换行, 语句块不换行
AllowShortFunctionsOnASingleLine: None
```

- 提交前运行 `clang-format -i`，禁止手工风格争论
- 注释：文件头用 Doxygen `@file/@brief`；对外 API 在 .h 中注释；实现内注释解释"为什么"而非"做什么"

## 4. 版本号与发布规范

- 固件版本：`主.次.修订`（SemVer）——主=不兼容变更，次=新功能，修订=修复
- 版本号唯一来源：`version.h` 中 `FW_VERSION_MAJOR/MINOR/PATCH`，编译时注入 git hash：
  ```c
  #define FW_VERSION  "1.2.3"
  #define FW_GIT_HASH GIT_HASH   /* -DGIT_HASH=\"$(git rev-parse --short HEAD)\" */
  ```
- 设备须支持查询版本（串口命令/上报字段），OTA 依据版本号防降级
- 发布固件命名：`产品名_v1.2.3_20260710.bin`，发布 tag 与 CHANGELOG 同步

## 5. Git 提交规范

- 提交信息：`类型(范围): 摘要`，类型取 `feat/fix/refactor/docs/test/chore`
  - 例：`feat(sht30): 支持周期测量模式`、`fix(ota): 修复分片重传偏移错误`
- 一类改动一个提交；格式化与逻辑修改分开提交
- 主分支保护，功能走分支 + 评审合入

## 6. 评审检查清单

- [ ] 命名符合第 1 节；无平台头文件泄漏进驱动本体
- [ ] 所有返回值被检查；错误路径资源正确释放（锁/片选）
- [ ] 无阻塞死等（超时机制齐全）；ISR 符合约定
- [ ] 缓冲区边界安全（长度参数、snprintf 而非 sprintf）
- [ ] volatile 正确使用于 ISR 共享变量
- [ ] 新增器件文档已同步（references + SKILL.md 索引）
