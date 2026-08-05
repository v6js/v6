public final class V6ProcessExit extends RuntimeException {
  public final int code;

  public V6ProcessExit(int code) {
    super(null, null, false, false);
    this.code = code;
  }
}
