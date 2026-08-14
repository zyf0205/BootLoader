# STM32F411 串口 Bootloader(YMODEM)

一个基于 **YMODEM 协议**的 STM32F411CEU6 串口 Bootloader:用任何支持 YMODEM 的终端软件(推荐 **Tera Term**)就能升级固件,不需要自研上位机。

## 它能干什么

```
电脑 (Tera Term)  ──USB转串口──> 开发板
   选择 .bin 文件, 点发送 ────────> 自动完成: 擦除 -> 写入 -> CRC 校验 -> 跳转新固件
```

- 工厂首次烧录后,之后升级**只插 USB 转串口,不用 ST-Link**
- 升级过程每包 CRC 校验,错了自动重传,中途断线也不会写坏 Flash
- 升级失败随时重来:设备会自动回到等待状态
- 固件带版本号(文件名 `app_v1.2.bin`),版本自动记录在 Flash 里

## 文档导航

| 文档 | 内容 | 什么时候看 |
|------|------|-----------|
| [快速上手](docs/快速上手.md) | 环境准备、编译烧录、第一次升级完整步骤 | **第一次用必看** |
| [架构详解](docs/架构详解.md) | 分层设计、启动/升级流程、代码阅读路线图 | 想改代码前看 |
| [YMODEM 协议说明](docs/YMODEM协议说明.md) | 协议帧格式、交互时序、差错恢复 | 想深入协议时看 |
| [常见问题](docs/常见问题.md) | 串口没输出、升级失败、改引脚等 | 遇到问题时看 |

## 目录结构

```
BootLoader/
├── bootloader/               # Bootloader 工程 (完整独立, 可直接拷贝)
│   ├── core/                 # 入口: main.c + 中断处理
│   ├── config/               # Flash 分区 / 超时等配置 / 版本号
│   ├── drivers/              # 串口(DMA)/Flash/CRC32 驱动
│   ├── protocols/ymodem/     # YMODEM 协议状态机 (纯 C, 不依赖硬件)
│   ├── app/                  # 启动决策 + 升级状态机
│   ├── modules/              # LED/按键/系统节拍/板级引脚定义
│   ├── libraries/            # CMSIS + StdPeriph 标准库
│   └── .eide/                # EIDE 工程配置
├── app/                      # APP 演示工程 (完整独立, 结构同上)
│                              # 功能: LED 闪烁 + 长按按键回到 Bootloader
├── docs/                     # 项目文档
├── tests/ymodem/             # 协议单元测试 (电脑上跑, 无需硬件)
├── tools/build_check.sh      # 编译验证脚本 (WSL 调用 Windows 工具链)
├── AGENTS.md                 # AI 助手构建参考 (普通人可忽略)
└── BootLoader.code-workspace # VSCode 工作区, 双击打开
```

## 快速开始 (30 秒版)

1. **编译烧录**:VSCode 打开 `BootLoader.code-workspace`(需装 EIDE 插件)→ 构建 `bootloader` 目标 → ST-Link 烧录
2. **构建固件**:构建 `app` 目标,得到 `app/build/app/App.bin`
3. **升级**:Tera Term 打开串口(115200)→ `File → Transfer → YMODEM → Send` → 选 App.bin → 完成

> 详细步骤、软件安装、首次部署流程见 [快速上手](docs/快速上手.md)。

## 硬件资源

| 资源 | 引脚 | 说明 |
|------|------|------|
| 调试串口 | PA9 (TX) / PA10 (RX) | USART1, 115200-8-N-1 |
| 状态 LED | PA1 | 低电平点亮 |
| 用户按键 | PC13 | 低电平按下 |
| 系统时钟 | HSI 16MHz → PLL 96MHz | 可切换 HSE 25MHz,见 `libraries/cmsis/system/system_stm32f4xx.h` |

## Flash 分区

| 区域 | 地址 | 大小 | 说明 |
|------|------|------|------|
| Bootloader | 0x08000000 | 16KB | 只读, ST-Link 烧录 |
| APP | 0x08004000 | 240KB | 串口升级写入 |
| 元数据 | 0x08040000 | 128KB | 只存 16B: 固件大小/CRC32/版本 |

## 开发环境

- Windows + VSCode + [EIDE](https://em-ide.com) 插件(工程管理、编译、烧录)
- Keil MDK v5.32 +(提供 AC5 编译器, EIDE 调用)
- Tera Term(串口升级)
- 测试:Linux/WSL + gcc(`cd tests/ymodem && make test`)
