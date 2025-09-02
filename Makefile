INCLUDE_DIR := include
EXTERNAL_DIR := external
ASSETS_DIR := assets
BUILD_DIR := build
PLATFORM_DIR := platform
MAIN := core/main.c

# Detect platform
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S), Darwin)
    PLATFORM := osx
else ifeq ($(UNAME_S), Linux)
    PLATFORM := linux
else ifeq ($(findstring MSYS_NT, $(UNAME_S)), MSYS_NT)
    PLATFORM := win32
else
    $(error Unsupported platform: $(UNAME_S))
endif

# Compiler settings based on platform
ifeq ($(PLATFORM), osx)
    CC := clang
    CFLAGS := -std=c23 -g -O3 -march=native -Wall -Wextra -Wno-unused-variable -Wno-unused-function
    LDLIBS := -framework Cocoa -framework AudioToolbox
else ifeq ($(PLATFORM), linux)
    CC := clang
    CFLAGS := -std=c23 -g -O3 -march=native -Wall -Wextra -Wno-unused-variable -Wno-unused-function
    LDLIBS := -lX11 -lm
else ifeq ($(PLATFORM), win32)
    CC := clang
    CFLAGS := -std=c23 -g -O3 -march=native -Wall -Wextra -Wno-unused-variable -Wno-unused-function
    LDLIBS := -lgdi32 -luser32 -lkernel32 -ldsound
    # LDFLAGS := -Wl,/SUBSYSTEM:WINDOWS
    TARGET_SUFFIX := .exe
endif

# Common include paths
CFLAGS += -I$(INCLUDE_DIR) -I$(EXTERNAL_DIR) -I$(ASSETS_DIR)
# Common flags
CFLAGS += -DDEBUG_ARENA_ALLOCATIONS=1 -DDEBUG_ARENA_RESETS=0

# Core source files
CORE_SRC := $(wildcard core/*.c)
EXTERNAL_SRC := external/olive.c

# Platform-specific sources
ifeq ($(PLATFORM), osx)
    PLATFORM_SRC := $(wildcard $(PLATFORM_DIR)/*/osx_*.c) \
                     $(wildcard $(PLATFORM_DIR)/*/osx_*.m)
else ifeq ($(PLATFORM), linux)
    PLATFORM_SRC := $(wildcard $(PLATFORM_DIR)/*/linux_*.c)
else ifeq ($(PLATFORM), win32)
    PLATFORM_SRC := $(wildcard $(PLATFORM_DIR)/*/win32_*.c)
endif

SRC := $(MAIN) $(CORE_SRC) $(EXTERNAL_SRC) $(PLATFORM_SRC)

# Target
TARGET := build/software-rendering-c$(TARGET_SUFFIX)

.PHONY: build run clean obj2c

all: build

build: $(SRC)
	mkdir -p build
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $(TARGET) $(LDLIBS)

run: build
	./$(TARGET)

obj2c:
	clang -Wall -Wextra -O2 -o obj2c$(TARGET_SUFFIX) external/obj2c/main.c

clean:
	rm -rf build
