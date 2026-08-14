CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -O2 -I../../bootloader/protocols/ymodem
SRC     = test_ymodem.c ../../bootloader/protocols/ymodem/ymodem.c ../../bootloader/protocols/ymodem/crc16.c
TARGET  = test_ymodem

all: $(TARGET)

$(TARGET): $(SRC) ../../bootloader/protocols/ymodem/ymodem.h
	$(CC) $(CFLAGS) -o $@ $(SRC)

test: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
