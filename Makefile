# CC := g++   #X86内核使用此编译器
CC := arm-linux-gnueabihf-g++   #ARM内核使用此编译器(需要先make clean)
CXXFLAGS := -w -std=c++11 -Wextra -g -I./include  #  -g代表生成的文件为dubug版本，在最终release版本需要去掉此参数
OBJ_DIR := obj
SRC_DIR := sources
INC_DIR := include
EXECUTABLE := macdaatr

SRCS := $(foreach dir, $(SRC_DIR), $(wildcard $(SRC_DIR)/*.cpp))#$(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRCS))

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJS)
	$(CC) $(CXXFLAGS) $^ -o $@ -lpthread

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp $(wildcard $(INC_DIR)/*.h)
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CXXFLAGS) -c $< -o $@

.PHONY: clean

clean:
	rm -rf $(OBJ_DIR)/*.o $(EXECUTABLE)

rebuild: clean all

.PHONY: rebuild






