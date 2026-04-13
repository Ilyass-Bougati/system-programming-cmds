CC = gcc
FLAGS = -Wall -Wextra -pedantic

clean:
	$(info Cleaning the build directory)
	rm -f *.o
	rm -f attr

attr: attr.c
	$(CC) -o attr attr.c include/*.c