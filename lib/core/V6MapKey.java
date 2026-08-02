public final class V6MapKey {
  final int tag;
  final Object ref;

  V6MapKey(int tag, Object ref) {
    this.tag = tag;
    this.ref = ref;
  }

  @Override
  public boolean equals(Object o) {
    return o instanceof V6MapKey && ((V6MapKey)o).ref == ref;
  }

  @Override
  public int hashCode() {
    return System.identityHashCode(ref);
  }
}
