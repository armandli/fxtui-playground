BUILD_DIR := build
BUILD_TYPE ?= Release

.PHONY: all configure build clean debug

all: configure build

configure:
	cmake -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)

build:
	cmake --build $(BUILD_DIR)

debug:
	cmake -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Debug
	cmake --build $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)
