public final class V6AsyncHooks {
  private V6AsyncHooks() {
  }

  private static V6Value fn(V6Callable c) {
    return new V6Value(V6Value.TAG_FUNC, 0, c);
  }

  private static V6Value objValue(V6Object o) {
    return new V6Value(V6Value.TAG_OBJ, 0, o);
  }

  private static V6Value num(double n) {
    return new V6Value(V6Value.TAG_NUM, n, null);
  }

  public static V6Object build() {
    V6Object o = new V6Object();

    o.set("createHook", fn((thisArg, args) -> {
            V6Object hook = new V6Object();
            hook.set("enable", fn((t, a) -> objValue(hook)));
            hook.set("disable", fn((t, a) -> objValue(hook)));
            return objValue(hook);
          }));

    o.set("executionAsyncId", fn((thisArg, args) -> num(0)));
    o.set("triggerAsyncId", fn((thisArg, args) -> num(0)));
    o.set("executionAsyncResource",
          fn((thisArg, args) -> objValue(new V6Object())));

    o.set("AsyncLocalStorage", objValue(new V6AsyncLocalStorageConstructor()));

    return o;
  }
}
