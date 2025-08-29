CC = clang
CFLAGS = -std=c23 -g -Wall -Wextra -Wno-unused-variable -Wno-unused-function -framework Cocoa
SRC = $(wildcard src/*.c src/*.m)
TARGET = build/software-rendering-c

.PHONY: all build clean

all: build

build: $(SRC)
	mkdir -p build
	$(CC) $(CFLAGS) $^ -o $(TARGET)

run: build
	$(TARGET)

clean:
	rm -rf build
