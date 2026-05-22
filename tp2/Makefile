CC = gcc
CFLAGS = -Wall -Wextra -O2

PROGS = myattr cdroit cowner cgroup suprimer
BINS = $(addprefix bin/, $(PROGS))

all: $(BINS)

bin:
	mkdir -p bin

bin/%: %.c | bin
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf bin
