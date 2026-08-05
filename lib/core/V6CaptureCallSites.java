public final class V6CaptureCallSites {
  private V6CaptureCallSites() {
  }

  private static V6Value objValue(V6Object o) {
    return new V6Value(V6Value.TAG_OBJ, 0, o);
  }

  private static V6Value fn(V6Callable c) {
    return new V6Value(V6Value.TAG_FUNC, 0, c);
  }

  public static final V6Value CAPTURE_CALL_SITES = fn((thisArg, args) -> {
    java.util
        .List<StackWalker.StackFrame> frames = StackWalker.getInstance().walk(
        s -> s.skip(1).limit(32).collect(java.util.stream.Collectors.toList()));
    V6Array result = new V6Array();
    for (StackWalker.StackFrame f : frames)
      result.push(
          objValue(new V6CallSiteObject(f.getMethodName(), f.getClassName())));
    return objValue(result);
  });
}
