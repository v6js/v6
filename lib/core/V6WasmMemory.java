import java.util.Arrays;

public final class V6WasmMemory {
  static final int PAGE_SIZE = 65536;

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

  public void copyWithin(int dst, int src, int len) {
    check(dst, len);
    check(src, len);
    System.arraycopy(data, src, data, dst, len);
  }

  public void fill(int dst, int val, int len) {
    check(dst, len);
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

  void check(int addr, int width) {
    if (addr < 0 || (long)addr + width > data.length)
      throw new RuntimeException("wasm: out of bounds memory access");
  }

  public int loadI32(int addr) {
    check(addr, 4);
    return (data[addr] & 0xFF) | ((data[addr + 1] & 0xFF) << 8) |
        ((data[addr + 2] & 0xFF) << 16) | ((data[addr + 3] & 0xFF) << 24);
  }

  public long loadI64(int addr) {
    check(addr, 8);
    long lo = loadI32(addr) & 0xFFFFFFFFL;
    long hi = loadI32(addr + 4) & 0xFFFFFFFFL;
    return lo | (hi << 32);
  }

  public float loadF32(int addr) {
    return Float.intBitsToFloat(loadI32(addr));
  }

  public double loadF64(int addr) {
    return Double.longBitsToDouble(loadI64(addr));
  }

  public int loadI32_8s(int addr) {
    check(addr, 1);
    return data[addr];
  }

  public int loadI32_8u(int addr) {
    check(addr, 1);
    return data[addr] & 0xFF;
  }

  public int loadI32_16s(int addr) {
    check(addr, 2);
    return (short)((data[addr] & 0xFF) | ((data[addr + 1] & 0xFF) << 8));
  }

  public int loadI32_16u(int addr) {
    check(addr, 2);
    return (data[addr] & 0xFF) | ((data[addr + 1] & 0xFF) << 8);
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
    check(addr, 4);
    data[addr] = (byte)val;
    data[addr + 1] = (byte)(val >> 8);
    data[addr + 2] = (byte)(val >> 16);
    data[addr + 3] = (byte)(val >> 24);
  }

  public void storeI64(int addr, long val) {
    check(addr, 8);
    storeI32(addr, (int)val);
    storeI32(addr + 4, (int)(val >> 32));
  }

  public void storeF32(int addr, float val) {
    storeI32(addr, Float.floatToRawIntBits(val));
  }

  public void storeF64(int addr, double val) {
    storeI64(addr, Double.doubleToRawLongBits(val));
  }

  public void storeI32_8(int addr, int val) {
    check(addr, 1);
    data[addr] = (byte)val;
  }

  public void storeI32_16(int addr, int val) {
    check(addr, 2);
    data[addr] = (byte)val;
    data[addr + 1] = (byte)(val >> 8);
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
