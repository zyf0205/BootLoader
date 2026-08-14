# STM32F411 串口 Bootloader (YMODEM)

基于 **YMODEM 协议**的 STM32F411CEU6 串口 Bootloader,使用任何支持 YMODEM 的终端工具(如 **Tera Term**)即可完成固件升级,无需自研上位机。

## 特性

- **YMODEM 协议**: 标准 CRC 模式, 1K 数据包, 兼容主流终端软件
- **可靠传输**: 每包 CRC16 校验 + 出错 NAK 重传 + 超时重试 + CAN 中止
- **三重 APP 校验**: 元数据魔数 + 堆栈指针 + Reset 向量
- **硬件 CRC32**: 升级完成后全量校验并存入元数据区
- **多种进入方式**: 按键保持复位 / APP 长按请求 / 无有效 APP 自动进入
- **DMA 收发**: 环形缓冲接收 + TC 中断非阻塞发送
- **状态 LED**: 慢闪=等待 / 快闪=接收 / 常亮=成功
- **平台无关协议层**: YMODEM 状态机零硬件依赖, 附带 Linux 单元测试

## 目录结构

```
├── bootloader/               # Bootloader 工程 (EIDE, 自包含)
│   ├── core/                 # 入口: main.c, 异常处理
│   ├── config/               # Flash 分区 / 行为配置 / 版本
│   ├── drivers/              # 驱动: usart (DMA), flash, crc32 (硬件)
│   ├── protocols/ymodem/     # YMODEM 状态机 + CRC16 (平台无关)
│   ├── app/                  # 应用层: boot 启动决策, updater 升级状态机
│   ├── modules/              # 板级模块: board/led/key/systick/bootapi
│   ├── libraries/            # CMSIS + StdPeriph 库
│   └── .eide/                # EIDE 工程配置
├── app/                      # APP 演示工程 (EIDE, 自包含, 结构同上)
│   ├── core/                 # main.c (LED 闪烁 + 长按进入 Bootloader)
│   ├── config/ modules/ libraries/ .eide/
├── tests/ymodem/             # YMODEM 状态机单元测试 (gcc)
├── tools/build_check.sh      # WSL 下调用 Windows AC5 的编译验证脚本
└── docs/                     # 架构与协议文档
```

> 两个工程完全独立, 各自携带所需的库与模块, 可直接拷贝使用。

## 快速开始

### 编译烧录

1. 用 VSCode 打开 `BootLoader.code-workspace`(需安装 **EIDE** 插件)
2. Bootloader 工程: 构建 `bootloader` 目标 -> 烧录 (ST-Link, 0x08000000)
3. APP 工程: 构建 `app` 目标 -> 生成 `build/app/App.bin`

> **首次部署推荐流程**: ST-Link 只烧录 Bootloader,然后按下方"串口升级"步骤用 YMODEM 发送 App.bin,一次性完成 APP 安装 + 元数据写入。
>
> 注意: 如果用 ST-Link 直接把 APP 烧到 0x08004000,元数据区仍是空的, Bootloader 会判定"无有效 APP"进入升级模式——这是预期行为(元数据是 APP 有效性的唯一权威),再走一次 YMODEM 升级即可正常跳转。

### 串口升级 (Tera Term)

1. USB 转串口连接开发板 (USART1: PA9-TX, PA10-RX, 115200-8-N-1)
2. 打开 Tera Term, 配置串口
3. 复位开发板:
   - 无有效 APP: 自动进入升级模式, 周期性发送 `C`
   - 有有效 APP: 5 秒窗口内开始传输, 或**按住 KEY 复位**强制进入
   - APP 运行中: **长按 KEY 2 秒**请求重入
4. 菜单 `File -> Transfer -> YMODEM -> Send...`
5. 选择固件 `build/app/App.bin` (勾选 CRC 校验)
6. 完成后自动跳转 APP

> 固件文件名含版本号时 (如 `app_v1.2.bin`) 会自动解析版本写入元数据。

### 单元测试

```bash
cd tests/ymodem && make test
```

### 编译验证 (WSL 调用 Windows Keil AC5, 无需 EIDE)

```bash
bash tools/build_check.sh
```

输出 `build/check/Bootloader.bin` (需 < 16KB) 与 `build/check/App.bin`。

## Flash 分区

| 区域       | 地址        | 大小  | Sector  | 用途         |
|------------|-------------|-------|---------|--------------|
| Bootloader | 0x08000000  | 16KB  | 0       | 启动 + YMODEM |
| APP        | 0x08004000  | 240KB | 1-5     | 应用程序     |
| Metadata   | 0x08040000  | 128KB | 6       | 固件信息 16B |
| Reserved   | 0x08060000  | 128KB | 7       | 预留         |

## 启动流程

```
复位
 ├─ 备份寄存器魔数? ── 是 ──> APP 请求重入 ──┐
 ├─ 按住 KEY? ──────── 是 ──> 按键触发 ──────┤
 ├─ APP 有效? ──────── 否 ──> 无有效 APP ────┼──> 升级模式 (无限等待 YMODEM)
 └─ 是: 5s 窗口等待 YMODEM ─┬─ 收到文件头 ────┘
                            └─ 超时 ──> 跳转 APP
```

## 开发环境

- VSCode + [EIDE](https://em-ide.com) 插件
- ARM Compiler 5 (AC5), microlib
- STM32F4xx StdPeriph 库
- 硬件: STM32F411CEU6 (Black Pill), 时钟 96MHz (HSI/HSE 可选)

## 文档

- [架构设计](docs/architecture.md)
- [YMODEM 协议说明](docs/ymodem-protocol.md)
