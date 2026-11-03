CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -Werror -O2 \
          -Imock \
          -I../../bootloader/drivers/flash \
          -I../../bootloader/drivers/crc32 \
          -Wno-int-to-pointer-cast
# -Wno-int-to-pointer-cast: flash.c 的 Flash_Write 以 uint32_t 地址做指针
# 运算, 在 32 位 ARM 上正确; 64 位主机上属预期差异, 其余警告仍按错误处理
# 编译真实的 flash.c; Flash 外设与 CRC32 外设由本测试的模拟层提供
SRC     = test_flash_valid.c ../../bootloader/drivers/flash/flash.c
TARGET  = test_flash_valid

all: $(TARGET)

$(TARGET): $(SRC) mock/flash.h mock/flash_config.h mock/stm32f4xx.h \
           ../../bootloader/drivers/flash/flash.h \
           ../../bootloader/drivers/crc32/crc32.h
	$(CC) $(CFLAGS) -o $@ $(SRC)

test: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(TARGET).exe
