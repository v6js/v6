import java.lang.invoke.MethodHandles;
import java.lang.invoke.VarHandle;
import java.nio.ByteOrder;
import java.util.Arrays;

public final class V6WasmMemory {
  static final int PAGE_SIZE = 65536;

  private static final VarHandle I32_VIEW =
      MethodHandles.byteArrayViewVarHandle(int[].class,
                                           ByteOrder.LITTLE_ENDIAN);
  private static final VarHandle I64_VIEW =
      MethodHandles.byteArrayViewVarHandle(long[].class,
                                           ByteOrder.LITTLE_ENDIAN);
  private static final VarHandle I16_VIEW =
      MethodHandles.byteArrayViewVarHandle(short[].class,
                                           ByteOrder.LITTLE_ENDIAN);

  byte[] data;
  int maxPages;

  public V6WasmMemory(int minPages, int maxPages) {
    this.data = new byte[minPages * PAGE_SIZE];
    this.maxPages = maxPages;
  }

  public int size() {
    return data.length / PAGE_SIZE;
  }

  public void initData(int offset, byte[] src) {
    System.arraycopy(src, 0, data, offset, src.length);
  }

  public void initData(int dstOffset, byte[] src, int srcOffset, int len) {
    System.arraycopy(src, srcOffset, data, dstOffset, len);
  }

  public void copyOut(int addr, byte[] dst, int dstOff, int len) {
    System.arraycopy(data, addr, dst, dstOff, len);
  }

  public void copyIn(int addr, byte[] src, int srcOff, int len) {
    System.arraycopy(src, srcOff, data, addr, len);
  }

  public void copyWithin(int dst, int src, int len) {
    System.arraycopy(data, src, data, dst, len);
  }

  public void fill(int dst, int val, int len) {
    Arrays.fill(data, dst, dst + len, (byte)val);
  }

  public int grow(int deltaPages) {
    int oldPages = size();
    if (deltaPages < 0)
      return -1;
    long newPages = (long)oldPages + (long)deltaPages;
    if (maxPages >= 0 && newPages > maxPages)
      return -1;
    if (newPages > 65536)
      return -1;
    data = Arrays.copyOf(data, (int)newPages * PAGE_SIZE);
    return oldPages;
  }

  public int loadI32(int addr) {
    return (int)I32_VIEW.get(data, addr);
  }

  public long loadI64(int addr) {
    return (long)I64_VIEW.get(data, addr);
  }

  public float loadF32(int addr) {
    return Float.intBitsToFloat(loadI32(addr));
  }

  public double loadF64(int addr) {
    return Double.longBitsToDouble(loadI64(addr));
  }

  public int loadI32_8s(int addr) {
    return data[addr];
  }

  public int loadI32_8u(int addr) {
    return data[addr] & 0xFF;
  }

  public int loadI32_16s(int addr) {
    return (short)I16_VIEW.get(data, addr);
  }

  public int loadI32_16u(int addr) {
    return ((short)I16_VIEW.get(data, addr)) & 0xFFFF;
  }

  public long loadI64_8s(int addr) {
    return loadI32_8s(addr);
  }

  public long loadI64_8u(int addr) {
    return loadI32_8u(addr);
  }

  public long loadI64_16s(int addr) {
    return loadI32_16s(addr);
  }

  public long loadI64_16u(int addr) {
    return loadI32_16u(addr);
  }

  public long loadI64_32s(int addr) {
    return loadI32(addr);
  }

  public long loadI64_32u(int addr) {
    return loadI32(addr) & 0xFFFFFFFFL;
  }

  public void storeI32(int addr, int val) {
    I32_VIEW.set(data, addr, val);
  }

  public void storeI64(int addr, long val) {
    I64_VIEW.set(data, addr, val);
  }

  public void storeF32(int addr, float val) {
    storeI32(addr, Float.floatToRawIntBits(val));
  }

  public void storeF64(int addr, double val) {
    storeI64(addr, Double.doubleToRawLongBits(val));
  }

  public void storeI32_8(int addr, int val) {
    data[addr] = (byte)val;
  }

  public void storeI32_16(int addr, int val) {
    I16_VIEW.set(data, addr, (short)val);
  }

  public void storeI64_8(int addr, long val) {
    storeI32_8(addr, (int)val);
  }

  public void storeI64_16(int addr, long val) {
    storeI32_16(addr, (int)val);
  }

  public void storeI64_32(int addr, long val) {
    storeI32(addr, (int)val);
  }
}
