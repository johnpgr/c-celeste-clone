#!/bin/bash

set -e

CC="clang"
C_FLAGS="-std=c23 \
        -g \
        -Wall \
        -Wextra \
        -Werror \
        -framework Cocoa"
INPUT="src/*.c src/*.m"
OUTPUT="build/software-rendering-c"

mkdir -p "build"

$CC $C_FLAGS $INPUT -o $OUTPUT
