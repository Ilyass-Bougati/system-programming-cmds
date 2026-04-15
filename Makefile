# Compiler and archiving tools
CC = gcc
AR = ar

CFLAGS = -Wall -Wextra -O2 -Iinclude
SRC_INC = $(wildcard include/*.c)
SRCS = $(SRC_INC)
OBJS = $(SRCS:.c=.o)
ATTR = attr

TARGET = mylib.a

all: $(TARGET)

$(TARGET): $(OBJS)
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

attr: attr.c $(TARGET)
	$(CC) $(CFLAGS) attr.c $(TARGET) -o $(ATTR)

clean:
	rm -f $(OBJS) $(TARGET) $(ATTR)