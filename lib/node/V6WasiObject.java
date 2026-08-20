import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.lang.reflect.Field;
import java.nio.charset.StandardCharsets;
import java.security.SecureRandom;
import java.util.ArrayList;
import java.util.List;

public class V6WasiObject extends V6Object {
  static final int ESUCCESS = 0;
  static final int EBADF = 8;
  static final int EINVAL = 28;

  final List<String> args = new ArrayList<>();
  final List<String> envPairs = new ArrayList<>();
  V6WasmMemory memory;
  boolean returnOnExit;
  final SecureRandom random = new SecureRandom();

  private static V6Value num(double d) {
    return new V6Value(V6Value.TAG_NUM, d, null);
  }

  void hookMemory(V6Value instanceValue) {
    if (instanceValue.tag() != V6Value.TAG_OBJ ||
        !(instanceValue.ref() instanceof V6WasmInstanceObject inst))
      throw new RuntimeException(
          "TypeError: wasi start/initialize expects a wasm instance");
    try {
      Field f = inst.compiledClass.getDeclaredField("wasmMemory0");
      f.setAccessible(true);
      memory = (V6WasmMemory)f.get(null);
    } catch (ReflectiveOperationException e) {
      throw new RuntimeException(
          "wasm module does not export memory required by WASI");
    }
  }

  private static int byteLen(String s) {
    return s.getBytes(StandardCharsets.UTF_8).length + 1;
  }

  private static int totalBufSize(List<String> items) {
    int total = 0;
    for (String s : items)
      total += byteLen(s);
    return total;
  }

  private void writeStringTable(int listPtr, int bufPtr, List<String> items) {
    int cur = bufPtr;
    for (int i = 0; i < items.size(); i++) {
      memory.storeI32(listPtr + i * 4, cur);
      byte[] bytes = items.get(i).getBytes(StandardCharsets.UTF_8);
      for (byte b : bytes)
        memory.storeI32_8(cur++, b);
      memory.storeI32_8(cur++, 0);
    }
  }

  V6Value argsSizesGet(V6Value[] a) {
    int argcPtr = (int)a[0].toNumber();
    int bufSizePtr = (int)a[1].toNumber();
    memory.storeI32(argcPtr, args.size());
    memory.storeI32(bufSizePtr, totalBufSize(args));
    return num(ESUCCESS);
  }

  V6Value argsGet(V6Value[] a) {
    int argvPtr = (int)a[0].toNumber();
    int argvBufPtr = (int)a[1].toNumber();
    writeStringTable(argvPtr, argvBufPtr, args);
    return num(ESUCCESS);
  }

  V6Value environSizesGet(V6Value[] a) {
    int countPtr = (int)a[0].toNumber();
    int bufSizePtr = (int)a[1].toNumber();
    memory.storeI32(countPtr, envPairs.size());
    memory.storeI32(bufSizePtr, totalBufSize(envPairs));
    return num(ESUCCESS);
  }

  V6Value environGet(V6Value[] a) {
    int environPtr = (int)a[0].toNumber();
    int environBufPtr = (int)a[1].toNumber();
    writeStringTable(environPtr, environBufPtr, envPairs);
    return num(ESUCCESS);
  }

  V6Value procExit(V6Value[] a) {
    throw new V6ProcessExit((int)a[0].toNumber());
  }

  V6Value fdWrite(V6Value[] a) {
    int fd = (int)a[0].toNumber();
    int iovsPtr = (int)a[1].toNumber();
    int iovsLen = (int)a[2].toNumber();
    int nwrittenPtr = (int)a[3].toNumber();
    if (fd != 1 && fd != 2)
      return num(EBADF);
    ByteArrayOutputStream out = new ByteArrayOutputStream();
    int total = 0;
    for (int i = 0; i < iovsLen; i++) {
      int base = iovsPtr + i * 8;
      int ptr = memory.loadI32(base);
      int len = memory.loadI32(base + 4);
      for (int j = 0; j < len; j++)
        out.write(memory.loadI32_8u(ptr + j));
      total += len;
    }
    try {
      byte[] data = out.toByteArray();
      if (fd == 1) {
        System.out.write(data);
        System.out.flush();
      } else {
        System.err.write(data);
        System.err.flush();
      }
    } catch (IOException e) {
      throw new RuntimeException(e);
    }
    memory.storeI32(nwrittenPtr, total);
    return num(ESUCCESS);
  }

  V6Value fdRead(V6Value[] a) {
    int fd = (int)a[0].toNumber();
    int nreadPtr = (int)a[3].toNumber();
    if (fd != 0)
      return num(EBADF);
    memory.storeI32(nreadPtr, 0);
    return num(ESUCCESS);
  }

  V6Value fdClose(V6Value[] a) {
    return num(ESUCCESS);
  }

  V6Value fdSeek(V6Value[] a) {
    return num(EINVAL);
  }

  V6Value fdFdstatGet(V6Value[] a) {
    int fd = (int)a[0].toNumber();
    int ptr = (int)a[1].toNumber();
    if (fd < 0 || fd > 2)
      return num(EBADF);
    memory.storeI32_8(ptr, 2);
    memory.storeI32_16(ptr + 2, 0);
    memory.storeI64(ptr + 8, -1L);
    memory.storeI64(ptr + 16, -1L);
    return num(ESUCCESS);
  }

  V6Value fdPrestatGet(V6Value[] a) {
    return num(EBADF);
  }

  V6Value clockTimeGet(V6Value[] a) {
    int clockId = (int)a[0].toNumber();
    int timePtr = (int)a[2].toNumber();
    long nanos = clockId == 1 ? System.nanoTime()
                              : System.currentTimeMillis() * 1_000_000L;
    memory.storeI64(timePtr, nanos);
    return num(ESUCCESS);
  }

  V6Value randomGet(V6Value[] a) {
    int bufPtr = (int)a[0].toNumber();
    int bufLen = (int)a[1].toNumber();
    byte[] bytes = new byte[bufLen];
    random.nextBytes(bytes);
    for (int i = 0; i < bufLen; i++)
      memory.storeI32_8(bufPtr + i, bytes[i]);
    return num(ESUCCESS);
  }
}
