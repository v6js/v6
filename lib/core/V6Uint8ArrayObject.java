public final class V6Uint8ArrayObject extends V6Object {
  public byte[] data;

  public V6Uint8ArrayObject(byte[] data) {
    setProto(V6Uint8ArrayConstructor.PROTOTYPE);
    this.data = data;
  }

  @Override
  public V6Value get(String key) {
    if (key.equals("length"))
      return new V6Value(V6Value.TAG_NUM, data.length, null);
    int idx = parseIndex(key);
    if (idx >= 0 && idx < data.length)
      return new V6Value(V6Value.TAG_NUM, data[idx] & 0xFF, null);
    return super.get(key);
  }

  @Override
  public void set(String key, V6Value value) {
    int idx = parseIndex(key);
    if (idx >= 0 && idx < data.length) {
      data[idx] = (byte)(int)value.toNumber();
      return;
    }
    if (idx >= 0)
      return;
    super.set(key, value);
  }

  @Override
  public V6Value getIndexed(int idx) {
    if (idx >= 0 && idx < data.length)
      return new V6Value(V6Value.TAG_NUM, data[idx] & 0xFF, null);
    return super.getIndexed(idx);
  }

  @Override
  public void setIndexed(int idx, V6Value value) {
    if (idx >= 0 && idx < data.length) {
      data[idx] = (byte)(int)value.toNumber();
      return;
    }
    if (idx >= 0)
      return;
    super.setIndexed(idx, value);
  }

  @Override
  public boolean has(String key) {
    int idx = parseIndex(key);
    if (idx >= 0)
      return idx < data.length;
    return super.has(key);
  }

  @Override
  public boolean delete(String key) {
    int idx = parseIndex(key);
    if (idx >= 0 && idx < data.length) {
      data[idx] = 0;
      return true;
    }
    return super.delete(key);
  }

  public byte[] toBytes() {
    return data;
  }
}
