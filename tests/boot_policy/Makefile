CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -Werror -O2 -I../../bootloader/app
SRC     = test_boot_policy.c ../../bootloader/app/boot_policy.c
TARGET  = test_boot_policy

all: $(TARGET)

$(TARGET): $(SRC) ../../bootloader/app/boot_policy.h
	$(CC) $(CFLAGS) -o $@ $(SRC)

test: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
