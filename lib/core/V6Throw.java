public final class V6Throw extends RuntimeException {
  public final V6Value value;

  public V6Throw(V6Value value) {
    super(null, null, false, false);
    this.value = value;
  }

  public static String formatUncaught(V6Value err) {
    if (err.tag() == V6Value.TAG_OBJ && err.ref() instanceof V6Object) {
      V6Value stack = ((V6Object)err.ref()).get("stack");
      if (stack.tag() == V6Value.TAG_STR)
        return stack.toString();
    }
    return err.toString();
  }
}
