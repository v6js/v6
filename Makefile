ifeq ($(origin CC),default)
  CC := clang
endif

JAVAC ?= javac

STD := -std=c11
WARN := -Wall -Wextra
INC := -Iinclude
CFLAGS ?= $(STD) $(WARN) $(INC) -g -O0

ifeq ($(OS),Windows_NT)
  EXE := .exe
  PLATFORM := windows
  CFLAGS += -D_CRT_SECURE_NO_WARNINGS
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
GEN := $(BUILD)/gen

RT_SRCS := $(filter-out src/main.c,$(wildcard src/*.c))
RT_OBJS := $(patsubst src/%.c,$(OBJ)/%.o,$(RT_SRCS))

TEST_SRCS := $(wildcard test/*.c)
TEST_OBJS := $(patsubst test/%.c,$(OBJ)/test_%.o,$(TEST_SRCS))

V6_BIN := $(BIN)/v6$(EXE)
TEST_BIN := $(BIN)/v6_test$(EXE)
PACK_CLASS_BIN := $(BIN)/pack_class$(EXE)

RUNTIME_CLASS_FILE := $(GEN)/V6Value.class
RUNTIME_CLASS_C := $(GEN)/runtime_class.c
OBJECT_CLASS_FILE := $(GEN)/V6Object.class
OBJECT_CLASS_C := $(GEN)/object_class.c

FMT_FILES := $(wildcard src/*.c) $(wildcard include/v6/*.h) $(wildcard test/*.c) $(wildcard test/*.h) $(wildcard tools/*.c) $(wildcard v6/*.java)

.PHONY: all test fmt clean dirs

all: $(V6_BIN)

$(V6_BIN): $(RT_OBJS) $(OBJ)/main.o $(OBJ)/runtime_class.o $(OBJ)/object_class.o | dirs
	$(CC) $(RT_OBJS) $(OBJ)/main.o $(OBJ)/runtime_class.o $(OBJ)/object_class.o -o $@ $(LDLIBS)

$(TEST_BIN): $(RT_OBJS) $(TEST_OBJS) $(OBJ)/runtime_class.o $(OBJ)/object_class.o | dirs
	$(CC) $(RT_OBJS) $(TEST_OBJS) $(OBJ)/runtime_class.o $(OBJ)/object_class.o -o $@ $(LDLIBS)

test: $(TEST_BIN)
	$(TEST_BIN)

$(PACK_CLASS_BIN): $(OBJ)/tool_pack_class.o | dirs
	$(CC) $(OBJ)/tool_pack_class.o -o $@

$(OBJ)/tool_pack_class.o: tools/pack_class.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

$(RUNTIME_CLASS_FILE) $(OBJECT_CLASS_FILE): v6/V6Value.java v6/V6Object.java | dirs
	$(JAVAC) -d $(GEN) v6/V6Value.java v6/V6Object.java

$(RUNTIME_CLASS_C): $(RUNTIME_CLASS_FILE) $(PACK_CLASS_BIN) | dirs
	$(PACK_CLASS_BIN) $(RUNTIME_CLASS_FILE) v6_runtime_class $(RUNTIME_CLASS_C)

$(OBJECT_CLASS_C): $(OBJECT_CLASS_FILE) $(PACK_CLASS_BIN) | dirs
	$(PACK_CLASS_BIN) $(OBJECT_CLASS_FILE) v6_object_class $(OBJECT_CLASS_C)

$(OBJ)/runtime_class.o: $(RUNTIME_CLASS_C) | dirs
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ)/object_class.o: $(OBJECT_CLASS_C) | dirs
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ)/%.o: src/%.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ)/test_%.o: test/%.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

dirs:
	@mkdir -p $(OBJ) $(BIN) $(GEN)

fmt:
	clang-format -i $(FMT_FILES)

clean:
	rm -rf $(BUILD)
