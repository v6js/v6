public final class V6WasmV128 {
  public final byte[] bytes;

  public V6WasmV128(byte[] bytes) {
    this.bytes = bytes;
  }

  public static V6WasmV128 zero() {
    return new V6WasmV128(new byte[16]);
  }
}
