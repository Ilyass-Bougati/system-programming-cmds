CC = gcc
AR = ar
CFLAGS = -Wall -Wextra -O2 -Iinclude

TARGET = mylib.a
SRC_INC = $(wildcard include/*.c)
OBJS = $(SRC_INC:.c=.o)

PROGS = myattr cdroit cowner cgroup suprimer
BINS = $(addprefix bin/, $(PROGS))


all: $(TARGET) $(BINS)

bin:
	mkdir -p bin

$(TARGET): $(OBJS)
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

bin/%: %.c $(TARGET) | bin
	$(CC) $(CFLAGS) $< $(TARGET) -o $@

clean:
	rm -f $(OBJS) $(TARGET)
	rm -rf bin