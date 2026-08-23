# STM32F411 YMODEM Bootloader

[![tests](https://github.com/zyf0205/BootLoader/actions/workflows/tests.yml/badge.svg)](https://github.com/zyf0205/BootLoader/actions/workflows/tests.yml)

面向 STM32F411CEU6 的串口 Bootloader。Bootloader 通过 USART1 接收 YMODEM 固件，写入 APP 分区，完成 CRC32 完整性验证并跳转运行；仓库同时提供一个 LED 闪烁演示 APP。

## 项目状态

- MCU：STM32F411CEU6，512 KB Flash，128 KB SRAM
- 外部晶振：12 MHz HSE，PLL 后系统时钟 96 MHz
- 串口：USART1，PA9/PA10，115200-8-N-1
- 协议：标准 YMODEM CRC16，支持 128 B SOH 和 1 KB STX 数据包
- 开发方式：Windows Keil MDK 或 VSCode + EIDE，均使用 ARM Compiler 5
- Agent 验证：WSL 调用 Windows AC5 工具链

## 目录

```text
BootLoader/
├── bootloader/              Bootloader 独立 Keil/EIDE 工程
│   ├── app/                 启动决策和升级流程
│   ├── config/              分区、超时和版本配置
│   ├── core/                main 和异常处理
│   ├── drivers/             USART DMA、Flash、CRC32
│   ├── protocols/ymodem/    平台无关 YMODEM 接收状态机
│   ├── modules/             board、LED、按键、SysTick、Boot API
│   └── libraries/           CMSIS 和 STM32 StdPeriph
├── app/                     演示 APP 独立 Keil/EIDE 工程
├── docs/                    构建、设计和升级文档
├── tests/                   YMODEM 与启动策略主机单元测试
├── tools/build_check.sh     WSL 调 Windows AC5 的全量验证脚本
└── BootLoader.code-workspace
```

## 快速开始

1. 用 VSCode + EIDE 打开 `BootLoader.code-workspace`，或分别用 Keil 打开两个子工程的 `.uvprojx`。
2. 构建 `bootloader` 工程，通过 ST-Link 烧录 Bootloader。
3. 构建 `app` 工程，得到 `app/build/app/App.bin`。
4. 串口工具选择 115200-8-N-1 和 `Ymodem`，发送 `App.bin`。发送器可自动选择 128 B SOH 或 1 KB STX 数据包。
5. 升级成功后 Bootloader 校验、提交元数据，1 秒后受控复位并重新校验后启动 APP。

正常复位且 APP 有效时会立即跳转，不再开放固定的串口等待窗口。进入升级模式的三种方式：上电/复位时按住 PC13 约 100 ms；APP 运行中长按 PC13 约 1.5 s（演示 APP 已内置，LED 快闪提示后复位，业务固件可直接调用 `Boot_RequestBootloader()`）；没有有效 APP 时自动进入恢复模式。

升级带防降级保护：APP 完好时文件名版本（`vX.Y`）低于已装版本的固件会被拒绝；文件名无版本号或处于恢复模式时不做限制，保证旧版本可救砖。Bootloader 另有 HardFault 复位循环保护，连续 3 次异常复位后停留在升级模式等待修复。

## 文档

- [开发与构建](docs/开发与构建.md)：EIDE、Keil、WSL Agent、输出文件和验证命令
- [架构详解](docs/架构详解.md)：分层架构图、升级时序图、YMODEM 状态机、并发设计与设计决策
- [系统设计](docs/系统设计.md)：Flash 分区、启动决策、升级状态机和跳转流程
- [升级与排错](docs/升级与排错.md)：YMODEM 操作、LED 状态和常见问题
- [CHANGELOG](CHANGELOG.md)：版本演进与显著变更

## 验证

```bash
bash tools/build_check.sh
make -C tests/ymodem test
make -C tests/boot_policy test
```

全量构建必须保证 Bootloader 小于 16 KB。构建和测试输出均被 Git 忽略。
