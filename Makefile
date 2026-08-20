ifeq ($(origin CC),default)
  CC := clang
endif

JAVAC ?= javac
PYTHON ?= python3

STD := -std=c11
WARN := -Wall -Wextra
INC := -Iinclude
DEPFLAGS := -MMD -MP
CFLAGS ?= $(STD) $(WARN) $(INC) $(DEPFLAGS)

ifeq ($(OS),Windows_NT)
  EXE := .exe
  PLATFORM := windows
  CFLAGS += -D_CRT_SECURE_NO_WARNINGS
  SHELL := cmd.exe
  MKDIR_P = if not exist "$(subst /,\,$1)" mkdir "$(subst /,\,$1)"
  RM_RF = if exist "$(subst /,\,$1)" rmdir /S /Q "$(subst /,\,$1)"
else
  EXE :=
  UNAME_S := $(shell uname -s)
  ifeq ($(UNAME_S),Darwin)
    PLATFORM := macos
  else
    PLATFORM := linux
  endif
  MKDIR_P = mkdir -p $1
  RM_RF = rm -rf $1
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
else ifeq ($(PLATFORM),windows)
  LDLIBS := -lws2_32
else
  LDLIBS :=
endif

ifeq ($(PLATFORM),windows)
  LDFLAGS += -Wl,/STACK:8388608
endif

BUILD_TYPE ?= debug

ifeq ($(BUILD_TYPE),release)
  BUILD := build/release
  CFLAGS += -O3 -DNDEBUG -flto
  LDFLAGS += -flto -fuse-ld=lld
else
  BUILD := build/debug
  CFLAGS += -g -O0
endif

OBJ := $(BUILD)/obj
BIN := $(BUILD)/bin
GEN := $(BUILD)/lib

RT_SRCS := $(filter-out src/main.c,$(wildcard src/*.c))
RT_OBJS := $(patsubst src/%.c,$(OBJ)/%.o,$(RT_SRCS))

TEST_SRCS := $(wildcard test/*.c)
TEST_OBJS := $(patsubst test/%.c,$(OBJ)/test_%.o,$(TEST_SRCS))

V6_BIN := $(BIN)/v6$(EXE)
TEST_BIN := $(BIN)/v6_test$(EXE)
PACK_CLASS_BIN := $(BIN)/pack_class$(EXE)

RT_JAVA_SRCS := lib/core/V6Value.java lib/core/V6Object.java lib/core/V6Shape.java lib/core/V6Array.java \
                lib/core/V6String.java lib/core/V6Number.java lib/core/V6Boolean.java \
                lib/core/V6BigInt.java \
                lib/core/V6Iterator.java lib/core/V6Ref.java lib/core/V6Callable.java \
                lib/core/V6Closure.java lib/core/V6Builtins.java \
                lib/core/V6CaptureCallSites.java lib/core/V6Class.java \
                lib/core/V6Throw.java lib/core/V6Rope.java lib/core/V6NativeConstructor.java \
                lib/core/V6MapObject.java lib/core/V6SetObject.java lib/core/V6MapKey.java \
                lib/core/V6MapConstructor.java lib/core/V6SetConstructor.java \
                lib/core/V6Symbol.java lib/core/V6SymbolFunction.java \
                lib/core/V6Generator.java lib/core/V6GeneratorFunction.java \
                lib/core/V6GeneratorReturn.java lib/core/V6MicrotaskQueue.java \
                lib/core/V6EventLoop.java lib/core/V6TimerTask.java \
                lib/core/V6Timers.java \
                lib/core/V6ProcessExit.java lib/core/V6DaemonClassLoader.java \
                lib/core/V6TaggedStream.java lib/core/V6Daemon.java \
                lib/core/V6ThreadStream.java lib/core/V6GlobalDispatchObject.java \
                lib/node/V6ProcessDispatchObject.java lib/core/V6EventLoopState.java \
                lib/core/V6Promise.java lib/core/V6PromiseConstructor.java \
                lib/core/V6AsyncFunction.java lib/core/V6AsyncGenerator.java \
                lib/core/V6AsyncGeneratorFunction.java \
                lib/core/V6Regex.java lib/core/V6RegexConstructor.java \
                lib/core/V6ErrorConstructor.java \
                lib/core/V6FunctionConstructor.java \
                lib/core/V6ArrayConstructor.java \
                lib/core/V6StringConstructor.java \
                lib/core/V6BooleanConstructor.java \
                lib/core/V6NumberConstructor.java \
                lib/core/V6Uint8ArrayObject.java \
                lib/core/V6Uint8ArrayConstructor.java \
                lib/core/V6CallSiteObject.java \
                lib/core/V6DateObject.java lib/core/V6DateConstructor.java \
                lib/core/V6Json.java \
                lib/core/V6WasmMemory.java \
                lib/core/V6WasmTable.java \
                lib/node/V6Path.java \
                lib/node/V6EventEmitterObject.java lib/node/V6EventEmitterConstructor.java \
                lib/node/V6NativeFunctionObject.java \
                lib/node/V6Util.java lib/node/V6Os.java lib/node/V6Process.java \
                lib/node/V6Tty.java \
                lib/node/V6Buffer.java lib/node/V6BufferConstructor.java \
                lib/node/V6Fs.java \
                lib/node/V6Assert.java lib/node/V6AssertFunction.java \
                lib/node/V6CallTrackerConstructor.java \
                lib/node/V6QueryString.java lib/node/V6PerfHooks.java \
                lib/node/V6Dns.java lib/node/V6DnsResolverConstructor.java \
                lib/node/V6StringDecoderObject.java \
                lib/node/V6StringDecoderConstructor.java \
                lib/node/V6UrlSearchParamsObject.java \
                lib/node/V6UrlSearchParamsConstructor.java \
                lib/node/V6UrlObject.java lib/node/V6UrlConstructor.java \
                lib/node/V6UrlLegacy.java \
                lib/node/V6Zlib.java lib/node/V6Crypto.java \
                lib/node/V6StreamMethods.java lib/node/V6StreamQueue.java \
                lib/node/V6StreamReadableConstructor.java \
                lib/node/V6StreamWritableConstructor.java \
                lib/node/V6StreamDuplexConstructor.java \
                lib/node/V6StreamTransformConstructor.java \
                lib/node/V6ChildProcess.java lib/node/V6Net.java lib/node/V6Http.java \
                lib/node/V6Readline.java lib/node/V6WorkerThreads.java \
                lib/node/V6WorkerConstructor.java \
                lib/node/V6MessageChannelConstructor.java \
                lib/node/V6TlsUtil.java lib/node/V6TrustAllManager.java lib/node/V6Tls.java \
                lib/node/V6HttpAgentConstructor.java lib/node/V6IpcUtil.java \
                lib/node/V6Cluster.java lib/node/V6Repl.java \
                lib/node/V6Dgram.java lib/node/V6Http2.java \
                lib/node/V6V8.java lib/node/V6ModuleModule.java \
                lib/node/V6DiagnosticsChannel.java lib/node/V6AsyncHooks.java \
                lib/node/V6AsyncLocalStorageConstructor.java \
                lib/node/V6UnsupportedConstructor.java \
                lib/node/V6Inspector.java lib/node/V6TraceEvents.java \
                lib/node/V6SparseModules.java \
                lib/shared/V6EventHandlerProperty.java \
                lib/web/V6EventObject.java lib/web/V6EventConstructor.java \
                lib/web/V6CustomEventObject.java lib/web/V6CustomEventConstructor.java \
                lib/web/V6EventTargetObject.java lib/web/V6EventTargetConstructor.java \
                lib/web/V6AbortSignalObject.java lib/web/V6AbortSignalConstructor.java \
                lib/web/V6AbortControllerConstructor.java \
                lib/web/V6StructuredClone.java \
                lib/web/V6TextEncoderConstructor.java \
                lib/web/V6TextDecoderObject.java lib/web/V6TextDecoderConstructor.java \
                lib/web/V6ReadableStreamObject.java lib/web/V6ReadableStreamConstructor.java \
                lib/web/V6WritableStreamObject.java lib/web/V6WritableStreamConstructor.java \
                lib/web/V6TransformStreamConstructor.java \
                lib/web/V6CountQueuingStrategyConstructor.java \
                lib/web/V6ByteLengthQueuingStrategyConstructor.java \
                lib/web/V6ArrayBufferObject.java lib/web/V6ArrayBufferConstructor.java \
                lib/web/V6BlobObject.java lib/web/V6BlobConstructor.java \
                lib/web/V6FileObject.java lib/web/V6FileConstructor.java \
                lib/web/V6FormDataObject.java lib/web/V6FormDataConstructor.java \
                lib/web/V6TextEncoderStreamConstructor.java \
                lib/web/V6TextDecoderStreamConstructor.java \
                lib/web/V6CompressionStreamConstructor.java \
                lib/web/V6DecompressionStreamConstructor.java \
                lib/web/V6HeadersObject.java lib/web/V6HeadersConstructor.java \
                lib/web/V6BodyUtil.java \
                lib/web/V6RequestObject.java lib/web/V6RequestConstructor.java \
                lib/web/V6ResponseObject.java lib/web/V6ResponseConstructor.java \
                lib/web/V6Fetch.java \
                lib/web/V6WebSocketObject.java lib/web/V6WebSocketListenerImpl.java \
                lib/web/V6WebSocketConstructor.java \
                lib/web/V6EventSourceObject.java lib/web/V6EventSourceConstructor.java \
                lib/web/V6CryptoKeyObject.java lib/web/V6CryptoKeyConstructor.java \
                lib/web/V6WebCrypto.java \
                lib/web/V6MessageEventObject.java lib/web/V6MessageEventConstructor.java \
                lib/web/V6MessagePortObject.java lib/web/V6MessagePortConstructor.java \
                lib/web/V6WebMessageChannelConstructor.java \
                lib/web/V6BroadcastChannelObject.java lib/web/V6BroadcastChannelConstructor.java \
                lib/web/V6WebWorkerObject.java lib/web/V6WebWorkerConstructor.java \
                lib/web/V6Navigator.java \
                lib/web/V6WebGlobals.java \
                lib/interop/V6JavaInterop.java lib/interop/V6JavaMarshal.java \
                lib/interop/V6JavaMatch.java \
                lib/interop/V6JavaClassObject.java lib/interop/V6JavaInstanceObject.java \
                lib/interop/V6JavaPackageObject.java lib/interop/V6JavaProxyHandler.java
RT_CLASS_FILES := $(patsubst lib/interop/%.java,$(GEN)/%.class,$(patsubst lib/web/%.java,$(GEN)/%.class,$(patsubst lib/shared/%.java,$(GEN)/%.class,$(patsubst lib/node/%.java,$(GEN)/%.class,$(patsubst lib/core/%.java,$(GEN)/%.class,$(RT_JAVA_SRCS))))))
RT_CLASS_NAMES := $(basename $(notdir $(RT_JAVA_SRCS)))
RT_GEN_C := $(patsubst %,$(GEN)/rt/%.c,$(RT_CLASS_NAMES))
RT_CLASS_OBJS := $(patsubst %,$(OBJ)/rt/%.o,$(RT_CLASS_NAMES))
RT_TABLE_C := $(GEN)/runtime_classes.c
GEN_ABS := $(abspath $(GEN))

GRADLE ?= gradle

FMT_FILES := $(wildcard src/*.c) $(wildcard include/v6/*.h) $(wildcard test/*.c) $(wildcard test/*.h) $(wildcard tools/*.c) $(wildcard lib/core/*.java) $(wildcard lib/node/*.java) $(wildcard lib/shared/*.java) $(wildcard lib/web/*.java) $(wildcard lib/interop/*.java)

BUILD_TOOL_BIN := build/tools/build_tool$(EXE)

.PHONY: all test fmt clean dirs release bench javaclasses target docs

.DEFAULT_GOAL := all

target:
	@$(call MKDIR_P,build/tools)
	$(CC) $(STD) $(WARN) -D_CRT_SECURE_NO_WARNINGS tools/build.c -o $(BUILD_TOOL_BIN)
	$(BUILD_TOOL_BIN)

all: | dirs
	$(MAKE) --no-print-directory BUILD_TYPE=$(BUILD_TYPE) javaclasses
	$(MAKE) --no-print-directory BUILD_TYPE=$(BUILD_TYPE) $(V6_BIN)

release:
	$(MAKE) BUILD_TYPE=release all

bench: release
	bash bench/run.sh

docs:
	$(PYTHON) docs/build.py

$(V6_BIN): $(RT_OBJS) $(OBJ)/main.o $(RT_CLASS_OBJS) $(OBJ)/runtime_classes.o | dirs
	$(CC) $(LDFLAGS) $(RT_OBJS) $(OBJ)/main.o $(RT_CLASS_OBJS) $(OBJ)/runtime_classes.o -o $@ $(LDLIBS)

$(TEST_BIN): $(RT_OBJS) $(TEST_OBJS) $(RT_CLASS_OBJS) $(OBJ)/runtime_classes.o | dirs
	$(CC) $(LDFLAGS) $(RT_OBJS) $(TEST_OBJS) $(RT_CLASS_OBJS) $(OBJ)/runtime_classes.o -o $@ $(LDLIBS)

test: | dirs
	$(MAKE) --no-print-directory BUILD_TYPE=$(BUILD_TYPE) javaclasses
	$(MAKE) --no-print-directory BUILD_TYPE=$(BUILD_TYPE) $(TEST_BIN)
	$(TEST_BIN)

$(PACK_CLASS_BIN): $(OBJ)/tool_pack_class.o | dirs
	$(CC) $(LDFLAGS) $(OBJ)/tool_pack_class.o -o $@

$(OBJ)/tool_pack_class.o: tools/pack_class.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

javaclasses: $(PACK_CLASS_BIN) | dirs
	$(GRADLE) -p lib compileJava -PdestDir=$(GEN_ABS) -q --console=plain
	$(PACK_CLASS_BIN) $(GEN)/rt $(RT_TABLE_C) $(RT_CLASS_FILES)

$(OBJ)/rt/%.o: $(GEN)/rt/%.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ)/runtime_classes.o: $(RT_TABLE_C) | dirs
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ)/%.o: src/%.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ)/test_%.o: test/%.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

dirs:
	@$(call MKDIR_P,$(OBJ))
	@$(call MKDIR_P,$(BIN))
	@$(call MKDIR_P,$(GEN))
	@$(call MKDIR_P,$(GEN)/rt)
	@$(call MKDIR_P,$(OBJ)/rt)

fmt:
	clang-format -i $(FMT_FILES)

clean:
	@$(call RM_RF,build)

-include $(wildcard $(OBJ)/*.d)
-include $(wildcard $(OBJ)/rt/*.d)
