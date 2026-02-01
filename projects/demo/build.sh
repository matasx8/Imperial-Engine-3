#!/usr/bin/env bash
set -e

# Directory of this script (the project folder)
PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$PROJECT_DIR/build"

# Default to Release build
BUILD_TYPE="Release"

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --release)
            BUILD_TYPE="Release"
            shift
            ;;
        *)
            echo "Unknown argument: $1"
            echo "Usage: $0 [--debug|--release]"
            exit 1
            ;;
    esac
done

mkdir -p "$BUILD_DIR"

# Configure the project using Ninja (must support compile commands)
cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" \
    -G "Ninja" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
