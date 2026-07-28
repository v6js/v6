public final class V6Array extends V6Object {
  public V6Array() {
    setProto(V6Builtins.ARRAY_PROTOTYPE);
  }

  @Override
  public String toString() {
    StringBuilder sb = new StringBuilder();
    for (int i = 0; i < length; i++) {
      if (i > 0)
        sb.append(",");
      V6Value v = get(Integer.toString(i));
      if (v.tag() != V6Value.TAG_UNDEF)
        sb.append(v.toString());
    }
    return sb.toString();
  }
}
