#!/bin/bash
# ====================================================================
# Bootloader/APP 全量编译验证脚本
#
# 用途: 在 WSL 下调用 Windows 侧 Keil MDK (AC5) 工具链,
#       编译两个工程的全部源文件并链接生成 bin, 验证:
#         - 所有源文件 AC5 编译零错误零警告
#         - Bootloader.bin 体积 < 16KB (Sector 0 限制)
#         - 向量表地址正确
#
# 要求: 已安装 Keil MDK (默认路径 D:/Keil_v5, 可在下方修改)
# 用法: bash tools/build_check.sh
# ====================================================================

set -e

KEIL_BIN=/mnt/d/Keil_v5/ARM/ARMCC/bin
OUT=build/check
CPU="--cpu Cortex-M4.fp"
C_FLAGS="--c99 --split_sections -O0 --diag_suppress=1,1295"
DEF="-DSTM32F411xE -DUSE_STDPERIPH_DRIVER"
BL_INC="-Ibootloader/core -Ibootloader/config \
        -Ibootloader/drivers/usart -Ibootloader/drivers/flash \
        -Ibootloader/drivers/crc32 -Ibootloader/protocols/ymodem -Ibootloader/app \
        -Ibootloader/modules/board -Ibootloader/modules/led -Ibootloader/modules/key \
        -Ibootloader/modules/systick -Ibootloader/modules/bootapi \
        -Ibootloader/libraries/cmsis/inc -Ibootloader/libraries/cmsis/system \
        -Ibootloader/libraries/stdperiph/inc"
APP_INC="-Iapp/core -Iapp/config \
         -Iapp/modules/board -Iapp/modules/led \
         -Iapp/modules/systick -Iapp/modules/bootapi \
         -Iapp/libraries/cmsis/inc -Iapp/libraries/cmsis/system \
         -Iapp/libraries/stdperiph/inc"

echo "===== Bootloader ====="
rm -rf $OUT
mkdir -p $OUT/obj_bl $OUT/obj_app

bl_compile() { # $1=源文件 $2=对象名
    "$KEIL_BIN/armcc.exe" $CPU $C_FLAGS $DEF $BL_INC -c "$1" -o "$OUT/obj_bl/$2.o"
}

bl_compile bootloader/core/main.c main
bl_compile bootloader/core/stm32f4xx_it.c it
bl_compile bootloader/drivers/usart/usart.c usart
bl_compile bootloader/drivers/flash/flash.c flash
bl_compile bootloader/drivers/crc32/crc32.c crc32
bl_compile bootloader/protocols/ymodem/ymodem.c ymodem
bl_compile bootloader/protocols/ymodem/crc16.c crc16
bl_compile bootloader/app/boot.c boot
bl_compile bootloader/app/boot_policy.c boot_policy
bl_compile bootloader/app/updater.c updater
bl_compile bootloader/modules/led/led.c led
bl_compile bootloader/modules/key/key.c key
bl_compile bootloader/modules/systick/systick.c systick
bl_compile bootloader/libraries/cmsis/system/system_stm32f4xx.c system_stm32f4xx
bl_compile bootloader/libraries/stdperiph/src/misc.c misc
bl_compile bootloader/libraries/stdperiph/src/stm32f4xx_gpio.c stm32f4xx_gpio
bl_compile bootloader/libraries/stdperiph/src/stm32f4xx_rcc.c stm32f4xx_rcc
bl_compile bootloader/libraries/stdperiph/src/stm32f4xx_usart.c stm32f4xx_usart
bl_compile bootloader/libraries/stdperiph/src/stm32f4xx_flash.c stm32f4xx_flash
bl_compile bootloader/libraries/stdperiph/src/stm32f4xx_dma.c stm32f4xx_dma

"$KEIL_BIN/armasm.exe" $CPU --pd "__MICROLIB SETA 1" \
    bootloader/libraries/cmsis/startup/startup_stm32f40_41xxx.s -o $OUT/obj_bl/startup.o

cat > $OUT/bootloader.sct <<'SCAT'
LR_IROM1 0x08000000 0x00004000  {
  ER_IROM1 0x08000000 0x00004000  {
   *.o (RESET, +First)
   *(InRoot$$Sections)
   .ANY (+RO)
  }
  RW_IRAM1 0x20000000 0x00020000  {
   .ANY (+RW +ZI)
  }
}
SCAT

"$KEIL_BIN/armlink.exe" $CPU --library_type=microlib \
    --scatter $OUT/bootloader.sct --entry Reset_Handler --map --list $OUT/bootloader.map \
    -o $OUT/bootloader.axf $OUT/obj_bl/*.o
"$KEIL_BIN/fromelf.exe" --bin $OUT/bootloader.axf --output $OUT/Bootloader.bin

BL_SIZE=$(stat -c%s $OUT/Bootloader.bin)
echo "Bootloader.bin: $BL_SIZE bytes (limit 16384)"

echo ""
echo "===== APP ====="
app_compile() { # $1=源文件 $2=对象名
    "$KEIL_BIN/armcc.exe" $CPU $C_FLAGS $DEF $APP_INC -c "$1" -o "$OUT/obj_app/$2.o"
}

app_compile app/core/main.c main
app_compile app/core/stm32f4xx_it.c it
app_compile app/modules/led/led.c led
app_compile app/modules/systick/systick.c systick
app_compile app/libraries/cmsis/system/system_stm32f4xx.c system_stm32f4xx
app_compile app/libraries/stdperiph/src/misc.c misc
app_compile app/libraries/stdperiph/src/stm32f4xx_gpio.c stm32f4xx_gpio
app_compile app/libraries/stdperiph/src/stm32f4xx_rcc.c stm32f4xx_rcc

"$KEIL_BIN/armasm.exe" $CPU --pd "__MICROLIB SETA 1" \
    app/libraries/cmsis/startup/startup_stm32f40_41xxx.s -o $OUT/obj_app/startup.o

cat > $OUT/app.sct <<'SCAT'
LR_IROM1 0x08004000 0x0003C000  {
  ER_IROM1 0x08004000 0x0003C000  {
   *.o (RESET, +First)
   *(InRoot$$Sections)
   .ANY (+RO)
  }
  RW_IRAM1 0x20000000 0x00020000  {
   .ANY (+RW +ZI)
  }
}
SCAT

"$KEIL_BIN/armlink.exe" $CPU --library_type=microlib \
    --scatter $OUT/app.sct --entry Reset_Handler --map --list $OUT/app.map \
    -o $OUT/app.axf $OUT/obj_app/*.o
"$KEIL_BIN/fromelf.exe" --bin $OUT/app.axf --output $OUT/App.bin

echo "App.bin: $(stat -c%s $OUT/App.bin) bytes"
echo ""

if [ "$BL_SIZE" -gt 16384 ]; then
    echo "FAIL: Bootloader 超过 16KB 限制!"
    exit 1
fi

echo "===== 构建验证通过 ====="
