CC=gcc
CFLAGS=-std=c99 -Wall -pedantic -g -L./src/lib64 -l runtime -fsanitize=address
all:
	mkdir -p build
	$(CC) $(CFLAGS) $(wildcard src/*.c) -o build/lab

clean:
	@rm -rf build
	
run: all
	LD_LIBRARY_PATH=./src/lib64 ./build/lab $(ARGS)
