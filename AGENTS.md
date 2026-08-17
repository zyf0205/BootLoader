# AGENTS.md - 项目构建与验证指南

## 项目概况

STM32F411CEU6 串口 Bootloader(YMODEM 协议),包含两个**自包含独立工程**:
- `bootloader/` - Bootloader 工程 (EIDE, 链接地址 0x08000000, 限 16KB)
- `app/` - APP 演示工程 (EIDE, 链接地址 0x08004000, LED + 长按重入 BL)

每个工程自带 `libraries/`(CMSIS + StdPeriph 全量)与 `modules/`(board/led/key/systick/bootapi),互不依赖。
开发板使用 12MHz HSE,两个工程均配置为 96MHz SYSCLK。

## 编译验证 (本机环境: WSL + Windows Keil AC5)

本机无 Linux 工具链,AC5 在 Windows 侧 (D:/Keil_v5, MDK 5.32 + ARM Compiler 5.06u7)。
WSL 可直接调用 Windows exe,但**必须用相对路径**(cwd 会被自动翻译成 Windows 路径,
命令行参数中的 /mnt/ 路径 Windows exe 不识别):

```bash
bash tools/build_check.sh
```

验证内容: 两个工程全部源文件 AC5 编译零错误零警告 + 链接 + bin 体积检查 (<16KB)。
输出到 `build/check/`(gitignored)。

YMODEM 协议单元测试 (Linux gcc, 与硬件无关):

```bash
cd tests/ymodem && make test
```

## AC5 关键编译知识 (踩过的坑)

1. **`--c99` 模式不支持 `__asm {}` 内嵌汇编**, 必须用 `__asm void` 函数形式 + `IMPORT`:
   ```c
   __asm void HardFault_Handler(void)
   {
       IMPORT HardFault_Handler_C
       ...
       B HardFault_Handler_C
   }
   ```
2. **microlib 工程汇编启动文件必须传 `--pd "__MICROLIB SETA 1"`**(armasm),
   否则链接报 `__use_two_region_memory` / `__initial_sp` 未定义。EIDE 勾选 use-microLIB 后自动处理。
3. microlib printf 重定向: `struct __FILE { int handle; }; FILE __stdout; int fputc(...)` (usart.c)
4. StdPeriph 此版本中 `FLASH_WaitForLastOperation` 是 flash.c 普通函数, flash_ramfunc.c 不需要
5. F411 PWR VOS 只有 bit14 (PWR_CR_VOS_0), 不是 F405 的 2 位

## Flash 分区

| 区域 | 地址 | 大小 | 说明 |
|------|------|------|------|
| Bootloader | 0x08000000 | 16KB | Sector 0 |
| APP | 0x08004000 | 240KB | Sector 1-5 |
| Metadata | 0x08040000 | 128KB | Sector 6, 存 16B 元数据 |

## 用户日常构建

用户使用 VSCode + EIDE 插件打开 `BootLoader.code-workspace` 构建烧录。
修改 `.eide/eide.yml` 后需同步更新 `tools/build_check.sh` 的源文件列表。
EIDE 配置是唯一 IDE 工程源。不要生成或提交 `.uvprojx`、子工程 workspace、`.cmsis/`、RTE 或模板工程；Agent 使用 `tools/build_check.sh` 调用 Windows AC5 验证。
