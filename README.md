# STM32F411 YMODEM Bootloader

面向 STM32F411CEU6 的串口 Bootloader。Bootloader 通过 USART1 接收 YMODEM 固件，写入 APP 分区，保存固件元数据并跳转运行；仓库同时提供一个 LED 闪烁和长按按键重入 Bootloader 的演示 APP。

## 项目状态

- MCU：STM32F411CEU6，512 KB Flash，128 KB SRAM
- 外部晶振：12 MHz HSE，PLL 后系统时钟 96 MHz
- 串口：USART1，PA9/PA10，115200-8-N-1
- 协议：YMODEM CRC16，支持 128 B（SOH）和 1 KB（STX）数据包
- 开发方式：Windows VSCode + EIDE + Keil MDK ARM Compiler 5
- Agent 验证：WSL 调用 Windows AC5 工具链

## 目录

```text
BootLoader/
├── bootloader/              Bootloader 独立 EIDE 工程
│   ├── app/                 启动决策和升级流程
│   ├── config/              分区、超时和版本配置
│   ├── core/                main 和异常处理
│   ├── drivers/             USART DMA、Flash、CRC32
│   ├── protocols/ymodem/    平台无关 YMODEM 接收状态机
│   ├── modules/             board、LED、按键、SysTick、Boot API
│   └── libraries/           CMSIS 和 STM32 StdPeriph
├── app/                     演示 APP 独立 EIDE 工程
├── docs/                    构建、设计和升级文档
├── tests/ymodem/            Linux 主机协议单元测试
├── tools/build_check.sh     WSL 调 Windows AC5 的全量验证脚本
└── BootLoader.code-workspace
```

## 快速开始

1. 在 Windows VSCode 中打开 `BootLoader.code-workspace`，安装 EIDE 插件。
2. 构建 `bootloader` 目标，通过 ST-Link 烧录 Bootloader。
3. 构建 `app` 目标，得到 `app/build/app/App.bin`。
4. 串口工具选择 115200-8-N-1 和 YMODEM，发送 `App.bin`。
5. 升级成功后 Bootloader 校验、保存元数据，并在 1 秒后跳转 APP。

首次启动没有有效 APP 时，Bootloader 无限等待传输；已有有效 APP 时等待 5 秒，未开始传输则跳转 APP。按住 PC13 后复位可强制进入升级模式，APP 运行时长按 PC13 约 2 秒也可请求重入。

## 文档

- [开发与构建](docs/开发与构建.md)：EIDE、Keil、WSL Agent、输出文件和验证命令
- [系统设计](docs/系统设计.md)：Flash 分区、启动决策、升级状态机和跳转流程
- [升级与排错](docs/升级与排错.md)：YMODEM 操作、LED 状态和常见问题

## 验证

```bash
bash tools/build_check.sh
make -C tests/ymodem test
```

全量构建必须保证 Bootloader 小于 16 KB。构建和测试输出均被 Git 忽略。
