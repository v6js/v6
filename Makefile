ifeq ($(origin CC),default)
  CC := clang
endif

JAVAC ?= javac

STD := -std=c11
WARN := -Wall -Wextra
INC := -Iinclude
DEPFLAGS := -MMD -MP
CFLAGS ?= $(STD) $(WARN) $(INC) $(DEPFLAGS)

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

BUILD_TYPE ?= debug

ifeq ($(BUILD_TYPE),release)
  BUILD := build/release
  CFLAGS += -O3 -DNDEBUG -flto
  LDFLAGS += -flto -fuse-ld=lld
else
  BUILD := build
  CFLAGS += -g -O0
endif

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

RT_JAVA_SRCS := lib/core/V6Value.java lib/core/V6Object.java lib/core/V6Array.java \
                lib/core/V6String.java lib/core/V6Number.java lib/core/V6Boolean.java \
                lib/core/V6Iterator.java lib/core/V6Ref.java lib/core/V6Callable.java \
                lib/core/V6Closure.java lib/core/V6Builtins.java lib/core/V6Class.java \
                lib/core/V6Throw.java lib/core/V6Rope.java lib/core/V6NativeConstructor.java \
                lib/core/V6MapObject.java lib/core/V6SetObject.java \
                lib/core/V6MapConstructor.java lib/core/V6SetConstructor.java \
                lib/core/V6Symbol.java lib/core/V6SymbolFunction.java \
                lib/core/V6Generator.java lib/core/V6GeneratorFunction.java \
                lib/core/V6GeneratorReturn.java lib/core/V6MicrotaskQueue.java \
                lib/core/V6Promise.java lib/core/V6PromiseConstructor.java \
                lib/core/V6AsyncFunction.java lib/core/V6AsyncGenerator.java \
                lib/core/V6AsyncGeneratorFunction.java \
                lib/core/V6Regex.java lib/core/V6RegexConstructor.java \
                lib/core/V6Json.java
RT_CLASS_FILES := $(patsubst lib/core/%.java,$(GEN)/%.class,$(RT_JAVA_SRCS))
RT_TABLE_C := $(GEN)/runtime_classes.c

FMT_FILES := $(wildcard src/*.c) $(wildcard include/v6/*.h) $(wildcard test/*.c) $(wildcard test/*.h) $(wildcard tools/*.c) $(wildcard lib/core/*.java)

.PHONY: all test fmt clean dirs release bench

all: $(V6_BIN)

release:
	$(MAKE) BUILD_TYPE=release all

bench: release
	bash bench/run.sh

$(V6_BIN): $(RT_OBJS) $(OBJ)/main.o $(OBJ)/runtime_classes.o | dirs
	$(CC) $(LDFLAGS) $(RT_OBJS) $(OBJ)/main.o $(OBJ)/runtime_classes.o -o $@ $(LDLIBS)

$(TEST_BIN): $(RT_OBJS) $(TEST_OBJS) $(OBJ)/runtime_classes.o | dirs
	$(CC) $(LDFLAGS) $(RT_OBJS) $(TEST_OBJS) $(OBJ)/runtime_classes.o -o $@ $(LDLIBS)

test: $(TEST_BIN)
	$(TEST_BIN)

$(PACK_CLASS_BIN): $(OBJ)/tool_pack_class.o | dirs
	$(CC) $(OBJ)/tool_pack_class.o -o $@

$(OBJ)/tool_pack_class.o: tools/pack_class.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

$(RT_CLASS_FILES): $(RT_JAVA_SRCS) | dirs
	$(JAVAC) -d $(GEN) $(RT_JAVA_SRCS)

$(RT_TABLE_C): $(RT_CLASS_FILES) $(PACK_CLASS_BIN) | dirs
	$(PACK_CLASS_BIN) $(RT_TABLE_C) $(RT_CLASS_FILES)

$(OBJ)/runtime_classes.o: $(RT_TABLE_C) | dirs
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
	rm -rf build

-include $(wildcard $(OBJ)/*.d)
