BUILD_DIR = ./build
SRC_FILES = ./src/*.c

main:
	gcc -o $(BUILD_DIR)/main $(SRC_FILES) -liio

clean:
	rm -rf BUILD_DIR/*