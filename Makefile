CC = gcc

BUILD_DIR = ./build
RELEASE   = $(BUILD_DIR)/main
DEBUG     = $(BUILD_DIR)/debug

SRC_FILES    = ./src/*.c
DEBUG_FLAGS  = -g
LINKER_FLAGS = -liio -lzmq

release:
	$(CC) -o $(RELEASE) $(SRC_FILES) $(LINKER_FLAGS)

debug:
	$(CC) -o $(DEBUG) $(SRC_FILES) $(LINKER_FLAGS) $(DEBUG_FLAGS)

clean:
	rm -rf $(BUILD_DIR)/*