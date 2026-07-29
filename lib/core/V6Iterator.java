public final class V6Iterator {
  private final V6Object arr;
  private final CharSequence str;
  private int idx = 0;
  private final int len;
  private final V6Callable nativeNext;
  private final V6Value nativeReceiver;
  private boolean nativeDone = false;
  private V6Value nativePending;
  private boolean nativeHasPending = false;

  public V6Iterator(V6Value v) {
    if (v.tag() == V6Value.TAG_STR) {
      str = (CharSequence)v.ref();
      arr = null;
      len = str.length();
      nativeNext = null;
      nativeReceiver = null;
    } else if (v.tag() == V6Value.TAG_OBJ) {
      V6Object obj = (V6Object)v.ref();
      V6Value nextFn = obj.get("next");
      if (nextFn.tag() == V6Value.TAG_FUNC && !(obj instanceof V6Array)) {
        str = null;
        arr = null;
        len = 0;
        nativeNext = nextFn.asCallable();
        nativeReceiver = v;
      } else {
        arr = obj;
        str = null;
        len = (int)arr.get("length").num();
        nativeNext = null;
        nativeReceiver = null;
      }
    } else {
      arr = null;
      str = null;
      len = 0;
      nativeNext = null;
      nativeReceiver = null;
    }
  }

  private void advanceNative() {
    V6Value result = nativeNext.call(nativeReceiver, new V6Value[0]);
    V6Object resultObj = (V6Object)result.ref();
    nativeDone = resultObj.get("done").truthy();
    nativePending = resultObj.get("value");
    nativeHasPending = true;
  }

  public boolean hasNext() {
    if (nativeNext != null) {
      if (!nativeHasPending)
        advanceNative();
      return !nativeDone;
    }
    return idx < len;
  }

  public V6Value next() {
    if (nativeNext != null) {
      if (!nativeHasPending)
        advanceNative();
      nativeHasPending = false;
      return nativePending;
    }
    if (str != null)
      return new V6Value(V6Value.TAG_STR, 0, String.valueOf(str.charAt(idx++)));
    if (arr != null)
      return arr.get(Integer.toString(idx++));
    idx++;
    return new V6Value(V6Value.TAG_UNDEF, 0, null);
  }
}
