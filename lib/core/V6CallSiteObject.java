public final class V6CallSiteObject extends V6Object {
  public V6CallSiteObject(String functionName, String fileName) {
    set("getFileName", fn((thisArg, args) -> strOrUndef(fileName)));
    set("getLineNumber", fn((thisArg, args) -> num(0)));
    set("getColumnNumber", fn((thisArg, args) -> num(0)));
    set("getFunctionName", fn((thisArg, args) -> strOrUndef(functionName)));
    set("getMethodName", fn((thisArg, args) -> strOrUndef(functionName)));
    set("getTypeName", fn((thisArg, args) -> strOrUndef(null)));
    set("getThis", fn((thisArg, args) -> V6Value.UNDEF));
    set("getFunction", fn((thisArg, args) -> V6Value.UNDEF));
    set("isEval", fn((thisArg, args) -> boolValue(false)));
    set("isNative", fn((thisArg, args) -> boolValue(false)));
    set("isConstructor", fn((thisArg, args) -> boolValue(false)));
    set("isToplevel", fn((thisArg, args) -> boolValue(false)));
    set("getEvalOrigin", fn((thisArg, args) -> V6Value.UNDEF));
    set("toString",
        fn((thisArg, args)
               -> new V6Value(
                   V6Value.TAG_STR, 0,
                   (functionName != null ? functionName : "<anonymous>") +
                       " (" + (fileName != null ? fileName : "<anonymous>") +
                       ")")));
  }

  private static V6Value fn(V6Callable c) {
    return new V6Value(V6Value.TAG_FUNC, 0, c);
  }

  private static V6Value strOrUndef(String s) {
    return s == null ? V6Value.UNDEF : new V6Value(V6Value.TAG_STR, 0, s);
  }

  private static V6Value num(double d) {
    return new V6Value(V6Value.TAG_NUM, d, null);
  }

  private static V6Value boolValue(boolean b) {
    return new V6Value(V6Value.TAG_BOOL, b ? 1 : 0, null);
  }
}
