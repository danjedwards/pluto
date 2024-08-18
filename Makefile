CC = gcc

BUILD_DIR = ./build
SRC_DIR   = ./src

RELEASE   = $(BUILD_DIR)/main
DEBUG     = $(BUILD_DIR)/debug
DSP_TEST  = $(BUILD_DIR)/dsp_test_exe

REL_OBJ_DIR = $(BUILD_DIR)/release_obj
DBG_OBJ_DIR = $(BUILD_DIR)/debug_obj

DEBUG_FLAGS  = -g
LINKER_FLAGS = -liio -lzmq -lm

SRC_FILES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS   = $(patsubst $(SRC_DIR)/%.c, $(REL_OBJ_DIR)/%.o, $(SRC_FILES))
DEPS      = $(wildcard $(SRC_DIR)/*.h)

ORDERED_OBJECTS = $(filter-out $(REL_OBJ_DIR)/main.o, $(OBJECTS)) $(REL_OBJ_DIR)/main.o
DEBUG_OBJECTS   = $(patsubst $(REL_OBJ_DIR)/%.o, $(DBG_OBJ_DIR)/%.o, $(ORDERED_OBJECTS))

release: $(ORDERED_OBJECTS)
	$(CC) -o $(RELEASE) $(ORDERED_OBJECTS) $(LINKER_FLAGS)

debug: $(DEBUG_OBJECTS)
	$(CC) -o $(DEBUG) $(DEBUG_OBJECTS) $(LINKER_FLAGS) $(DEBUG_FLAGS)

dsp_test: $(BUILD_DIR)/dsp.o
	$(CC) -o $(DSP_TEST) $(BUILD_DIR)/dsp.o $(LINKER_FLAGS) $(DEBUG_FLAGS) -DDSP_TARGET

$(REL_OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(DEPS)
	@mkdir -p $(REL_OBJ_DIR)
	$(CC) -c $< -o $@

$(DBG_OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(DEPS)
	@mkdir -p $(DBG_OBJ_DIR)
	$(CC) -c $< -o $@ $(DEBUG_FLAGS)

clean:
	rm -rf $(BUILD_DIR)/*