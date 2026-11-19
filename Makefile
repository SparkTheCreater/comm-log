# 默认用本机编译器。交叉编译示例：
# make CXX=aarch64-linux-gnu-g++ AR=aarch64-linux-gnu-ar

CXX ?= g++
AR ?= ar

CXXFLAGS ?= -std=c++11 -Wall -fPIC -g -pthread
CXXFLAGS += -I./include

LIB_NAME = libcomm_log.a
SRC_DIR = src
OBJ_DIR = obj

SRCS_CPP = $(wildcard $(SRC_DIR)/*.cpp)
OBJS = $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRCS_CPP))

.PHONY: all clean create_dirs

all: create_dirs $(LIB_NAME)

create_dirs:
	@mkdir -p $(OBJ_DIR)

$(LIB_NAME): $(OBJS)
	$(AR) rcs $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(LIB_NAME)
