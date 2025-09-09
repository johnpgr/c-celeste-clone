PROJECT_NAME := c-celeste-clone
INCLUDE_DIR := include
EXTERNAL_DIR := external
PLATFORM_DIR := platform
ASSETS_DIR := assets
BUILD_DIR := build

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
# Add flags to automatically generate header dependencies
CFLAGS := -g -std=c23 -Wall -Wextra -Wno-unused-variable -Wno-unused-function -MMD -MP

ifeq ($(PLATFORM), osx)
    LDLIBS := -framework Cocoa -framework AudioToolbox -framework OpenGL
else ifeq ($(PLATFORM), linux)
    LDLIBS := -lX11 -lXext -lm -lpulse -lpulse-simple -lGL
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

OBJ_DIR := $(BUILD_DIR)/obj/$(BUILD_MODE)
BIN_DIR := $(BUILD_DIR)/bin/$(BUILD_MODE)

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

# Creates a list of .o files that will be placed in the build directory.
OBJ := $(patsubst %.c, $(OBJ_DIR)/%.o, $(notdir $(filter %.c,$(SRC))))
OBJ += $(patsubst %.m, $(OBJ_DIR)/%.o, $(notdir $(filter %.m,$(SRC))))

DEPS := $(OBJ:.o=.d)

# Target
TARGET := $(BIN_DIR)/$(PROJECT_NAME)$(TARGET_SUFFIX)

.PHONY: build run clean release

all: build
build: $(TARGET)

# It depends on all object files '$(OBJ)'.
# If any object file is newer, this rule re-links them.
$(TARGET): $(OBJ)
	@echo "Linking..."
	@mkdir -p $(BIN_DIR)
	$(CC) $^ -o $@ $(LDFLAGS) $(LDLIBS)

# Use make's built-in wildcard function to find all immediate subdirectories.
PLATFORM_SUBDIRS := $(wildcard $(PLATFORM_DIR)/*)

# The VPATH tells 'make' where to find the source files.
VPATH := src external $(PLATFORM_SUBDIRS)

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(OBJ_DIR)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# MODIFIED: Rule for macOS Objective-C files (if any)
$(OBJ_DIR)/%.o: %.m
	@mkdir -p $(OBJ_DIR)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# This makes sure files are recompiled even if only a header file changes.
-include $(DEPS)

release:
	$(MAKE) RELEASE=1

run: build
	./$(TARGET)

clean:
	rm -rf build
