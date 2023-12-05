appName ?= rid-transmitter
version ?= 1.0.0
architecture ?= arm64
outputPath ?= $(shell pwd)/output

all:
	@astyle --quiet --options=astylerc src/*.cpp,*.hpp
	@cmake -Bbuild -H.; cmake --build build -j 12
	@size build/$(appName)

clean:
	@rm -rf build install log $(outputPath) __pycache__
	@echo "All build artifacts removed"

.PHONY: all docker clean
