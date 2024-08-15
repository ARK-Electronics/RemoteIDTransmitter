all:
	@astyle --quiet --options=astylerc src/*.cpp,*.hpp
	@cmake -Bbuild -H.; cmake --build build -j 12
	@size build/rid-transmitter

clean:
	@rm -rf build
	@echo "All build artifacts removed"

.PHONY: all docker clean
