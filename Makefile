PROJECT_NAME := c-celeste-clone
INCLUDE_DIR := include
EXTERNAL_DIR := external
ASSETS_DIR := assets
BUILD_DIR := build
PLATFORM_DIR := platform

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

CC := clang
CFLAGS := -g -std=c23 -Wall -Wextra -Wno-unused-variable -Wno-unused-function

ifeq ($(PLATFORM), osx)
    LDLIBS := -framework Cocoa -framework AudioToolbox
else ifeq ($(PLATFORM), linux)
    LDLIBS := -lX11 -lXext -lm -lpulse -lpulse-simple
else ifeq ($(PLATFORM), win32)
	LDLIBS := -lgdi32 -luser32 -ldsound -lopengl32
    LDFLAGS := -Wl,/SUBSYSTEM:WINDOWS
    TARGET_SUFFIX := .exe
endif

ifdef RELEASE
    CFLAGS += -O3 -DNDEBUG
    BUILD_MODE := release
else
    CFLAGS += -DDEBUG_MODE=1
    BUILD_MODE := debug
endif


# Our include paths
CFLAGS += -I$(INCLUDE_DIR) -I$(EXTERNAL_DIR) -I$(ASSETS_DIR)

# Source files
MAIN_SRC := $(wildcard src/*.c)
EXTERNAL_SRC := $(wildcard external/*.c)

# Platform-specific sources
ifeq ($(PLATFORM), osx)
    PLATFORM_SRC := $(wildcard $(PLATFORM_DIR)/*/osx_*.c) \
                     $(wildcard $(PLATFORM_DIR)/*/osx_*.m)
else ifeq ($(PLATFORM), linux)
    PLATFORM_SRC := $(wildcard $(PLATFORM_DIR)/*/linux_*.c)
else ifeq ($(PLATFORM), win32)
    PLATFORM_SRC := $(wildcard $(PLATFORM_DIR)/*/win32_*.c)
endif

SRC := $(MAIN_SRC) $(EXTERNAL_SRC) $(PLATFORM_SRC)

# Target
TARGET := build/$(BUILD_MODE)/$(PROJECT_NAME)$(TARGET_SUFFIX)

.PHONY: build run clean release

all: build

build: $(SRC)
	mkdir -p build/$(BUILD_MODE)
	$(CC) $(CFLAGS) $^ -o $(TARGET) $(LDFLAGS) $(LDLIBS)

release:
	$(MAKE) RELEASE=1

run: build
	./$(TARGET)

clean:
	rm -rf build
