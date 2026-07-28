public final class V6Iterator {
  private final V6Object arr;
  private final CharSequence str;
  private int idx = 0;
  private final int len;

  public V6Iterator(V6Value v) {
    if (v.tag() == V6Value.TAG_STR) {
      str = (CharSequence)v.ref();
      arr = null;
      len = str.length();
    } else if (v.tag() == V6Value.TAG_OBJ) {
      arr = (V6Object)v.ref();
      str = null;
      len = (int)arr.get("length").num();
    } else {
      arr = null;
      str = null;
      len = 0;
    }
  }

  public boolean hasNext() {
    return idx < len;
  }

  public V6Value next() {
    if (str != null)
      return new V6Value(V6Value.TAG_STR, 0, String.valueOf(str.charAt(idx++)));
    if (arr != null)
      return arr.get(Integer.toString(idx++));
    idx++;
    return new V6Value(V6Value.TAG_UNDEF, 0, null);
  }
}
