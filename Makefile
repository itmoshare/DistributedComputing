CC=clang
CFLAGS=-std=c99 -Wall -pedantic
all:
	mkdir -p build
	$(CC) $(CFLAGS) $(wildcard src/*.c) -o build/lab

clean:
	@rm -rf build
	
run: all
	./build/lab $(ARGS)
