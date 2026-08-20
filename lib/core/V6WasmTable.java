import java.util.Arrays;

public final class V6WasmTable {
  int[] elems;
  int maxSize;

  public V6WasmTable(int minSize, int maxSize) {
    this.elems = new int[minSize];
    Arrays.fill(this.elems, -1);
    this.maxSize = maxSize;
  }

  public int size() {
    return elems.length;
  }

  public int get(int idx) {
    if (idx < 0 || idx >= elems.length)
      throw new RuntimeException("wasm: table index out of bounds");
    return elems[idx];
  }

  public void set(int idx, int funcIdx) {
    if (idx < 0 || idx >= elems.length)
      throw new RuntimeException("wasm: table index out of bounds");
    elems[idx] = funcIdx;
  }

  public int grow(int delta, int initFuncIdx) {
    int oldSize = elems.length;
    if (delta < 0)
      return -1;
    long newSize = (long)oldSize + (long)delta;
    if (maxSize >= 0 && newSize > maxSize)
      return -1;
    elems = Arrays.copyOf(elems, (int)newSize);
    Arrays.fill(elems, oldSize, (int)newSize, initFuncIdx);
    return oldSize;
  }
}
