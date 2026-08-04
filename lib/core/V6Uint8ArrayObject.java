public final class V6Uint8ArrayObject extends V6Object {
  public V6Uint8ArrayObject(byte[] bytes) {
    setProto(V6Uint8ArrayConstructor.PROTOTYPE);
    for (byte b : bytes)
      push(new V6Value(V6Value.TAG_NUM, b & 0xFF, null));
  }

  public byte[] toBytes() {
    int n = (int)get("length").num();
    byte[] out = new byte[n];
    for (int i = 0; i < n; i++)
      out[i] = (byte)(int)get(Integer.toString(i)).toNumber();
    return out;
  }
}
