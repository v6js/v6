ifeq ($(origin CC),default)
  CC := clang
endif

STD := -std=c11
WARN := -Wall -Wextra
INC := -Iinclude
CFLAGS ?= $(STD) $(WARN) $(INC) -g -O0

ifeq ($(OS),Windows_NT)
  EXE := .exe
  PLATFORM := windows
else
  EXE :=
  UNAME_S := $(shell uname -s)
  ifeq ($(UNAME_S),Darwin)
    PLATFORM := macos
  else
    PLATFORM := linux
  endif
endif

ifneq ($(JAVA_HOME),)
  ifeq ($(PLATFORM),windows)
    JNI_OS_DIR := win32
  else ifeq ($(PLATFORM),macos)
    JNI_OS_DIR := darwin
  else
    JNI_OS_DIR := linux
  endif
  CFLAGS += -DV6_HAVE_JNI -I"$(JAVA_HOME)/include" -I"$(JAVA_HOME)/include/$(JNI_OS_DIR)"
endif

ifeq ($(PLATFORM),linux)
  LDLIBS := -ldl
else
  LDLIBS :=
endif

BUILD := build
OBJ := $(BUILD)/obj
BIN := $(BUILD)/bin

RT_SRCS := $(filter-out src/main.c,$(wildcard src/*.c))
RT_OBJS := $(patsubst src/%.c,$(OBJ)/%.o,$(RT_SRCS))

TEST_SRCS := $(wildcard test/*.c)
TEST_OBJS := $(patsubst test/%.c,$(OBJ)/test_%.o,$(TEST_SRCS))

V6_BIN := $(BIN)/v6$(EXE)
TEST_BIN := $(BIN)/v6_test$(EXE)

FMT_FILES := $(wildcard src/*.c) $(wildcard include/v6/*.h) $(wildcard test/*.c) $(wildcard test/*.h)

.PHONY: all test fmt clean dirs

all: $(V6_BIN)

$(V6_BIN): $(RT_OBJS) $(OBJ)/main.o | dirs
	$(CC) $(RT_OBJS) $(OBJ)/main.o -o $@ $(LDLIBS)

$(TEST_BIN): $(RT_OBJS) $(TEST_OBJS) | dirs
	$(CC) $(RT_OBJS) $(TEST_OBJS) -o $@ $(LDLIBS)

test: $(TEST_BIN)
	$(TEST_BIN)

$(OBJ)/%.o: src/%.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ)/test_%.o: test/%.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

dirs:
	@mkdir -p $(OBJ) $(BIN)

fmt:
	clang-format -i $(FMT_FILES)

clean:
	rm -rf $(BUILD)
