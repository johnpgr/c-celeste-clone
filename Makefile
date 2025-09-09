# ==============================================================================
# C Celeste Clone - Build Configuration
# ==============================================================================

PROJECT_NAME := c-celeste-clone

# ==============================================================================
# Directory Structure
# ==============================================================================

INCLUDE_DIR := include
EXTERNAL_DIR := external
PLATFORM_DIR := platform
ASSETS_DIR := assets
BUILD_DIR := build

# ==============================================================================
# Platform Detection
# ==============================================================================

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

# ==============================================================================
# Build Mode Configuration
# ==============================================================================

ifdef RELEASE
    BUILD_MODE := release
    OPTIMIZATION_FLAGS := -O3 -DNDEBUG
    LINK_DEBUG_FLAGS :=
else
    BUILD_MODE := debug
    OPTIMIZATION_FLAGS := -g -DDEBUG_MODE=1
    LINK_DEBUG_FLAGS := -g
endif

# ==============================================================================
# Output Directories
# ==============================================================================

BUILD_MODE_DIR := $(BUILD_DIR)/$(BUILD_MODE)
OBJ_DIR := $(BUILD_MODE_DIR)/obj
BIN_DIR := $(BUILD_MODE_DIR)/bin
LIB_DIR := $(BUILD_MODE_DIR)/lib

# ==============================================================================
# Compiler Configuration
# ==============================================================================

CC := clang
BASE_CFLAGS := -std=c23 -Wall -Wextra -Wno-unused-variable -Wno-unused-function -MMD -MP
INCLUDE_FLAGS := -I$(INCLUDE_DIR) -I$(EXTERNAL_DIR) -I$(ASSETS_DIR)
CFLAGS := $(BASE_CFLAGS) $(OPTIMIZATION_FLAGS) $(INCLUDE_FLAGS)

# ==============================================================================
# Platform-Specific Linker Configuration
# ==============================================================================

ifeq ($(PLATFORM), osx)
    LDLIBS := -framework Cocoa -framework AudioToolbox -framework OpenGL
    TARGET_SUFFIX :=
else ifeq ($(PLATFORM), linux)
    LDLIBS := -lX11 -lXext -lm -lpulse -lpulse-simple -lGL
    TARGET_SUFFIX :=
else ifeq ($(PLATFORM), win32)
    LDLIBS := -lgdi32 -luser32 -ldsound -lopengl32
    LDFLAGS := -Wl,/SUBSYSTEM:WINDOWS
    TARGET_SUFFIX := .exe
endif

LDFLAGS += -fuse-ld=lld

# ==============================================================================
# Source File Discovery
# ==============================================================================

# Main application source files
# MAIN_SRC := $(wildcard src/*.c)
MAIN_SRC := src/main.c

# External library source files
EXTERNAL_SRC := $(wildcard external/*.c)

# Platform-specific source files
ifeq ($(PLATFORM), osx)
    PLATFORM_SRC := $(wildcard $(PLATFORM_DIR)/*/osx_*.c) \
                     $(wildcard $(PLATFORM_DIR)/*/osx_*.m)
else ifeq ($(PLATFORM), linux)
    PLATFORM_SRC := $(wildcard $(PLATFORM_DIR)/*/linux_*.c)
else ifeq ($(PLATFORM), win32)
    PLATFORM_SRC := $(wildcard $(PLATFORM_DIR)/*/win32_*.c)
endif

# Combined source files
SRC := $(MAIN_SRC) $(EXTERNAL_SRC) $(PLATFORM_SRC)

# ==============================================================================
# Object File and Dependency Generation
# ==============================================================================

# Convert source files to object file paths
OBJ := $(addprefix $(OBJ_DIR)/,$(notdir $(patsubst %.c,%.o,$(filter %.c,$(SRC))))) \
       $(addprefix $(OBJ_DIR)/,$(notdir $(patsubst %.m,%.o,$(filter %.m,$(SRC)))))

# Dependency files for incremental builds
DEPS := $(OBJ:.o=.d)

# Categorize object files for library creation
EXTERNAL_OBJ := $(filter %/external/%.o,$(OBJ))
PLATFORM_OBJ := $(filter %/platform/%.o,$(OBJ))
MAIN_OBJ := $(filter-out $(EXTERNAL_OBJ) $(PLATFORM_OBJ),$(OBJ))

# ==============================================================================
# Target Definitions
# ==============================================================================

TARGET := $(BIN_DIR)/$(PROJECT_NAME)$(TARGET_SUFFIX)
EXTERNAL_LIB := $(LIB_DIR)/libexternal.a
PLATFORM_LIB := $(LIB_DIR)/libplatform.a

# ==============================================================================
# Build Rules
# ==============================================================================

.PHONY: all build run clean release help

# Default target
all: build

# Main build target
build: $(TARGET)

# Final executable linking
$(TARGET): $(MAIN_OBJ) $(EXTERNAL_LIB) $(PLATFORM_LIB)
	@mkdir -p $(dir $@)
	@echo "Linking $(PROJECT_NAME)..."
	$(CC) $^ -o $@ $(LDFLAGS) $(LINK_DEBUG_FLAGS) $(LDLIBS)
	@echo "Build complete: $@"

# External dependencies static library
$(EXTERNAL_LIB): $(EXTERNAL_OBJ)
	@mkdir -p $(LIB_DIR)
	@echo "Creating external library..."
	ar rcs $@ $^

# Platform-specific code static library
$(PLATFORM_LIB): $(PLATFORM_OBJ)
	@mkdir -p $(LIB_DIR)
	@echo "Creating platform library..."
	ar rcs $@ $^

# ==============================================================================
# Compilation Rules
# ==============================================================================

# Set up source search paths
PLATFORM_SUBDIRS := $(wildcard $(PLATFORM_DIR)/*)
VPATH := src external $(PLATFORM_SUBDIRS)

# C source compilation
$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Objective-C source compilation (macOS)
$(OBJ_DIR)/%.o: %.m
	@mkdir -p $(dir $@)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Include dependency files for incremental builds
-include $(DEPS)

# ==============================================================================
# Utility Targets
# ==============================================================================

# Build release version
release:
	@echo "Building release version..."
	$(MAKE) RELEASE=1

# Build and run the application
run: build
	@echo "Running $(PROJECT_NAME)..."
	./$(TARGET)

# Clean all build artifacts
clean:
	@echo "Cleaning build directory..."
	rm -rf $(BUILD_DIR)

# Display help information
help:
	@echo "Available targets:"
	@echo "  build    - Build the project (default)"
	@echo "  release  - Build optimized release version"
	@echo "  run      - Build and run the application"
	@echo "  clean    - Remove all build artifacts"
	@echo "  help     - Show this help message"
	@echo ""
	@echo "Current configuration:"
	@echo "  Platform:    $(PLATFORM)"
	@echo "  Build mode:  $(BUILD_MODE)"
	@echo "  Target:      $(TARGET)"
