# 更新日志

本项目的显著变更记录。版本号遵循 `主.次.修订`，与
`bootloader/config/version.h` 保持一致。

## 未发布

可靠性审查修复版本（代码审查发现的 1 项设计缺口、2 项意图偏差与
3 项加固）。

### 修复

- **HardFault 循环保护覆盖 APP 侧 Fault（设计缺口）**：原 BKP4R 计数
  仅由 Bootloader 自身的 HardFault 处理器写入，APP 跳转后向量表重定向，
  APP 内 Fault 永不计数——有 bug 的新固件会 Fault-复位-循环且保护
  永不触发。`boot_api.h` 新增 `Boot_ReportFaultReset()`（双工程契约
  同步更新），APP 的 `HardFault_Handler` 经其上报；连续达到
  `BOOT_FAULT_LIMIT` 后停在升级模式。
- **防降级改用真实 APP 完整性**：`on_header` 防降级检查原以"元数据
  有效"近似"APP 完好"（硬编码 `app_valid=1`），元数据有效但固件损坏
  （如 SWD 直刷裸固件）时旧版本救砖被误拦。现经 `Flash_IsAppValid()`
  全量校验（元数据 + 向量表 + CRC32）后再判定，与代码注释及
  boot_policy 决策表的语义一致。
- **升级完成标志语义修正（文档）**：BKP3R 实际行为是"跳过一次按键
  检测后照常校验并启动新 APP"，`boot.c` 注释与《系统设计》启动流程图
  此前误写为"进入升级模式"。

### 新增

- **元数据记录自身 CRC**：`fw_meta_t` 由 16B 扩展为 20B，新增
  `meta_crc32` 字段（前 16 字节的 CRC32），`Flash_LoadMeta` 校验以
  拦截元数据字段位翻转（防降级所依赖的 version 不再裸奔）。
  `meta_crc32` 为擦除态 0xFFFFFFFF 的记录按旧 16B 格式兼容放行。
  **注意**：旧格式元数据在本次升级后首次启动会被视为无效进入
  recovery，重新发送一次 App.bin 即完成迁移。

### 改进

- RX 溢出检测仅在 `WAITING`/`RECEIVING` 态生效：升级已提交
  （SUCCESS）后的残留垃圾不再重开会话擦除刚写入的固件。
- `Restart()` 恢复 LED 慢闪，中止/失败回到等待期不再残留接收期快闪。
- **Fault 计数改为闩锁语义**：进入升级模式不再清零 BKP4R，仅在正常
  启动跳转前与升级成功后清零。原行为下连续 3 次 Fault 进入 recovery
  后计数立即归零，断电重上电又会尝试跳转坏固件。
- BL 与 APP 侧 Fault 计数上限统一为 0xFFFF（原 BL 侧封顶
  `BOOT_FAULT_LIMIT`，APP 侧 0xFFFF，语义不对称）。
- `Usart_FlushRx()` 同时清除 RX 溢出标志，避免丢弃脏数据后再次
  触发多余的重启会话。
- `BootPolicy_AllowUpgrade` 调用改为传入真实的 `app_valid`，不再
  依赖外层短路与硬编码 `1`。
- `fw_meta_t` 增加编译期断言（`sizeof == 20`），防止未来加字段时
  静默改变 `meta_crc32` 的覆盖范围。
- `BL_VERSION_STR` 由 `MAJOR/MINOR/PATCH` 派生，消除字符串与数值
  手工同步的重复陷阱；删除未被引用的 `BL_VERSION` 数值宏与
  `BOOT_BL_VERSION_*`。

### 测试

- `tests/updater` 新增 2 用例（5 → 7）：元数据有效但 APP 损坏时旧版本
  放行救砖；SUCCESS 态 RX 溢出忽略。模拟层 `Flash_IsAppValid` 增加
  "APP 损坏"注入开关，`fw_meta_t` 与固件侧同步 20B。
- **新增 `tests/flash_valid`（10 用例）**：编译真实的 `flash.c`，用
  软件 CRC-32/MPEG-2 替换硬件 CRC 外设、模拟 Flash 替换 StdPeriph
  接口，覆盖启动校验的每个拒绝分支（魔数、MSP、Reset 向量与 Thumb
  位、size 边界、整包 CRC、meta_crc32 位翻转、旧 16B 兼容、尾部补
  0xFF）。为此 `crc32.c` 增加 `Crc32_Accumulate`/`Crc32_Result`
  薄封装，`flash.c` 不再直接访问 `CRC->DR`。
- 三个测试套件启用 `-Werror`。

### 清理

- 删除孤儿文件：两个子工程 `.code-workspace`、两个空模板
  `.eide/files.options.yml`、两个与根配置重复的 `.eide/.gitignore`。
- 删除零调用接口 `ymodem_abort()`。
- 新增 `.gitattributes` 统一换行符（脚本/Makefile 强制 LF，
  `.uvprojx` 保持 CRLF）。
- README 增加架构图、技术亮点与"设计取舍与已知局限"小节。


## v3.3.1 - 2026-08-23

测试背书与代码清理版本。

### 测试

- **新增 `tests/updater` 掉电安全序列单测（5 用例）**：编译真实的
  updater.c + ymodem.c + boot_policy.c，仅以模拟 Flash 替换驱动层。
  模拟层记录全部 Flash 操作顺序、强制"编程只能 1→0"的物理特性，
  并在**每一次 Flash 操作后断言断电不变量**"元数据有效 ⇒ 固件已
  完整写入"——完整升级流程中不变量被检查 43 次，等价于在每个可能
  的断电时刻验证安全性。覆盖：操作顺序（失效→擦除→写入→提交）、
  防降级拒绝不触碰 APP 区、传输截断不提交元数据、元数据写失败后
  可重试、RX 溢出重启接收。

### 清理

- 移除 TX DMA 通路死代码（约 110 行：`Usart_Send`、
  DMA2_Stream7 中断与缓冲）：同时消除"高优先级中断忙等低优先级
  中断清除标志"的潜在死锁路径。Bootloader 体积 13120 → 12272 B。
- 移除未用接口：`Updater_Finished`、`Crc32_CalcWords/CalcBytes`、
  `LED_BLINK_RAPID/LED_HEARTBEAT`（led 模块双工程同步）。
- 明确错误码约定（AGENTS.md）：驱动层领域错误枚举（flash_err_t）、
  回调/协议层 int（0 成功/非 0 失败）、布尔查询 uint8_t。
- CI 流水线纳入 updater 测试。

## v3.3.0 - 2026-08-22

可靠性修复与安全特性版本。侧重：修复答辩级协议缺陷、补全防变砖
链路、新增防降级保护、闭环 APP 软件重入入口。

### 修复

- **中断上下文竞态（P0）**：`ymodem_tick()` 原运行于 SysTick 中断，
  与主循环 `ymodem_feed()` 并发读写协议状态且可能在中断里执行
  printf 等非重入回调。现中断仅累积 1ms 节拍计数，协议状态机由
  主循环独占访问。
- **半包永久挂死（P0）**：发送端发出包起始后死机/拔线时，接收端
  卡在半包状态且超时逻辑被短路。新增 1s 帧内超时，丢弃半包并要求
  重传。
- **YMODEM 序号 255→0 回绕（P0）**：SOH（128 B 包）传输超过
  255 包（31.8 KB）时必然失败。现 RX_DATA 态下 seq 0 按数据包
  处理，符合协议 8-bit 回绕语义。
- **超长文件名误解析（P0）**：文件名 ≥63 字节未终止时被静默截断，
  导致大小字段从文件名中间误解析。现整包拒绝。
- **升级窗口超时死状态（P0）**：超时后状态机空转。现超时后 APP
  有效则跳转、否则系统复位。
- RX 环形缓冲溢出静默覆盖：DMA 覆盖未读数据时检测并重启接收
  会话；`Usart_Putc` 忙等增加超时兜底。
- 包内 `CAN CAN`：校验失败的包扫描原始字节流，识别发送端中途
  取消并立即中止，不再重传 5 次超时（约 15s）。
- DONE 后杂散字节不再组包应答；文件头 size 解析限 9 位防溢出。

### 新增

- **防降级保护**：新增 `BootPolicy_AllowUpgrade` 纯函数（含 7 条
  决策表单测）。APP 完好时拒绝低于已装版本的固件；recovery 态
  （APP 无效）与版本未知（文件名无 `vX.Y`）时放行，保证可救砖。
- **HardFault 复位循环保护**：RTC BKP4R 记录连续 Fault 复位次数，
  达到 3 次后不再自动跳 APP，停留在升级模式等待修复；正常启动或
  进入升级模式时清零。
- **APP 长按重入升级**：演示 APP 接入按键模块，长按 USER 键
  （PC13）1.5 s 触发 `Boot_RequestBootloader()` 写 BKP 魔数后
  复位进入升级；短按不动作防误触，长按期间 LED 快闪提示。

### 改进

- 跳转 APP 前彻底清理外设：GPIOA/GPIOC 整端口复位
  （`RCC_DeInit` 不复位 GPIO 配置），AHB1/APB1/APB2 外设时钟
  使能位显式清零。
- `Verifying CRC32` 误导文案修正为 `Calculating CRC32`。
- 单元测试扩充：YMODEM 15 个用例（新增 seq 回绕/帧内超时/包内
  CAN），启动策略 13 条断言（新增防降级决策表）。
- Bootloader 体积 13120 B / 16 KB 限制。

## v3.2.2 - 稳定版

- 修复 12 MHz HSE 时钟配置与 YMODEM 升级流程（`c95fd73`）。
- 恢复双工程 Keil 项目配置，修复 EIDE 输出并生成 BIN
  （`e9b1031`、`19a2864`）。
- 重写项目开发与升级文档（`a1504e0`）。
- 实测可完整走通 YMODEM 升级与跳转，作为稳定基线。

## v3.0.0

- 重构为 YMODEM 协议 + 自包含双工程结构（`4628fb7`）：
  Bootloader（16 KB，Sector 0）与演示 APP（0x08004000 起，
  240 KB），元数据两阶段提交（magic 最后写入），启动决策纯函数
  化，BKP 双寄存器软件重入 API，主机端单元测试。
