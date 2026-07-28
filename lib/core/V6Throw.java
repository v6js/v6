public final class V6Throw extends RuntimeException {
  public final V6Value value;

  public V6Throw(V6Value value) {
    super(null, null, false, false);
    this.value = value;
  }
}
