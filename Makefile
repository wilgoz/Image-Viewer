OBJS            = img-viewer.cc
CC              = g++
COMPILE_FLAGS   = -Wall -Wextra -std=c++17 -lstdc++fs -Wpedantic -O2
LINKER_FLAGS    = -lSDL2 -lSDL2_image
OBJ_NAME        = img-viewer

all: $(OBJS)
	$(CC) $(OBJS) $(COMPILE_FLAGS) $(LINKER_FLAGS) -o $(OBJ_NAME)