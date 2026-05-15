CC      = gcc
CFLAGS  = -Wall -Wextra

SRCS    = $(wildcard *.c)
BINS    = $(SRCS:.c=)

all: $(BINS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(BINS)

.PHONY: all clean
