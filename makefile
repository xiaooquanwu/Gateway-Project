#编译器
CC = gcc

#编译选项
CFLAGS = -Wall -pedantic -std=gnu99 -g

#最后可执行文件
TARGET = main

#.cpp文件
SRCS += $(wildcard uart/*.c)
SRCS += $(wildcard user/*.c)
SRCS += $(wildcard coapClient/*.c)
SRCS += $(wildcard coapServer/*.c)
SRCS += $(wildcard comm/*.c)
SRCS += $(wildcard iptable/*.c)
SRCS += $(wildcard TFTPServer/*.c)
SRCS += $(wildcard sqlite/*.c)

OBJS = $(SRCS:.c=.o)

DLIBS = -lmraa -lzlog -lpthread -lsqlite3

INC = -I /usr/local/include/ \
	-I ./user \
	-I ./sqlite \
	-I ./uart \
	-I ./coapClient \
	-I ./coapServer \
	-I ./comm \
	-I ./iptable \
	-I ./TFTPServer

$(TARGET) : $(OBJS)
	$(CC) $(INC) -o $@ $^ $(DLIBS)

clean:
	rm -rf $(TARGET) $(OBJS)

exec : clean $(TARGET)
	@echo start
	./$(TARGET)
	@echo end

%.o : %.c
	$(CC) $(CFLAGS) $(INC) -o $@ -c $<
