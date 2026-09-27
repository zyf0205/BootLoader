CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -O2 \
          -Imock \
          -I../../bootloader/app \
          -I../../bootloader/config \
          -I../../bootloader/protocols/ymodem \
          -I../../bootloader/modules/led \
          -I../../bootloader/drivers/usart \
          -I../../bootloader/modules/systick
SRC     = test_updater.c \
          ../../bootloader/app/updater.c \
          ../../bootloader/app/boot_policy.c \
          ../../bootloader/protocols/ymodem/ymodem.c \
          ../../bootloader/protocols/ymodem/crc16.c
# 产物名避开 "update" 字样: 无 manifest 的 exe 名含 update 会被
# Windows UAC 误判为安装程序 (错误 740), 导致 make test 无法运行
TARGET  = test_powerfail

all: $(TARGET)

$(TARGET): $(SRC) mock/flash.h mock/flash_config.h mock/stm32f4xx.h \
           ../../bootloader/app/updater.h \
           ../../bootloader/protocols/ymodem/ymodem.h
	$(CC) $(CFLAGS) -o $@ $(SRC)

test: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
