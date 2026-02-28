# Sofanthiel Makefile

# Compiler settings
CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic -O2
LDFLAGS :=

# Directories
SRC_DIR := sofanthiel
BUILD_DIR := build
BIN_DIR := bin

# Platform-specific settings
ifeq ($(OS),Windows_NT)
    # Windows-specific settings for MSYS2
    MINGW_PATH ?= /mingw64
    SDL3_CFLAGS := -I"$(MINGW_PATH)/include/SDL3" -I"$(MINGW_PATH)/include"
    
    # Try to detect which SDL3main library is available
    SDL3MAIN_LIB := 
    ifneq ($(wildcard $(MINGW_PATH)/lib/libSDL3main.a),)
        SDL3MAIN_LIB := -lSDL3main
    else ifneq ($(wildcard $(MINGW_PATH)/lib/libSDL3_main.a),)
        SDL3MAIN_LIB := -lSDL3_main
    else
        # No SDL3main library found - skip it
        SDL3MAIN_LIB := 
    endif
    
    # Fixed library order: SDL3 libs BEFORE dependent libs, system libs LAST
    SDL3_LDFLAGS := -L"$(MINGW_PATH)/lib" -lmingw32 \
                    -limgui -lnfd \
                    $(SDL3MAIN_LIB) -lSDL3 -lSDL3_image \
                    -lgdi32 -lole32 -loleaut32 -luuid -lcomctl32 -lcomdlg32 -lshell32 \
                    -limm32 -lsetupapi -lwinmm -lversion
    EXE := $(BIN_DIR)/sofanthiel.exe
    MKDIR_BUILD := mkdir -p $(BUILD_DIR)
    MKDIR_BIN := mkdir -p $(BIN_DIR)
    RM := rm -f
    RM_DIR := rm -rf
else
    SDL3_CFLAGS := $(shell pkg-config --cflags sdl3 sdl3-image gtk+-3.0) -I/usr/local/include
    SDL3_LDFLAGS := -L/usr/local/lib \
                    -Wl,-Bstatic -lSDL3 -limgui -lnfd -Wl,-Bdynamic \
                    -lSDL3_image \
                    $(shell pkg-config --libs gtk+-3.0) \
                    -ldl -lpthread -lm -lX11 -lXext -lXrandr -lXi -lXfixes -lXcursor -lXss
    EXE := $(BIN_DIR)/sofanthiel
    MKDIR_BUILD := mkdir -p $(BUILD_DIR)
    MKDIR_BIN := mkdir -p $(BIN_DIR)
    RM := rm -f
    RM_DIR := rm -rf
endif

# Add SDL flags to compiler and linker flags
CXXFLAGS += $(SDL3_CFLAGS)
LDFLAGS += $(SDL3_LDFLAGS)

# Find all source files
SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(SRCS:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

# Main target
all: $(EXE)

# Create build directory
$(BUILD_DIR):
	$(MKDIR_BUILD)

# Create bin directory
$(BIN_DIR):
	$(MKDIR_BIN)

# Compile source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Link object files
$(EXE): $(OBJS) | $(BIN_DIR)
	$(CXX) $^ -o $@ $(LDFLAGS)

# Clean build files
clean:
	$(RM) $(OBJS)
	$(RM) $(EXE)

# Clean everything
distclean: clean
	$(RM_DIR) $(BUILD_DIR)
	$(RM_DIR) $(BIN_DIR)

.PHONY: all clean distclean