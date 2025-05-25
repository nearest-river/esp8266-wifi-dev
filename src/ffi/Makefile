
CC=xtensa-lx106-elf-gcc
CFLAGS=-c
OUT=build
FILES=*.c lwip/*.c os/*.c core/*.c newlib/*.c
LIB_NAME=esp8266-sys-core
LIB_PATH=$(OUT)/lib$(LIB_NAME).a

all:
	rm -rf $(OUT)
	mkdir -p $(OUT)
	$(CC) $(CFLAGS) $(FILES)
	mv *.o $(OUT)
	ar -rc $(LIB_PATH) $(OUT)/*.o
	cp $(LIB_PATH) ../../lib/






