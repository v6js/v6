public final class V6AsyncLocalStorageConstructor
    extends V6Object implements V6NativeConstructor {
  private static V6Value fn(V6Callable c) {
    return new V6Value(V6Value.TAG_FUNC, 0, c);
  }

  private static V6Value objValue(V6Object o) {
    return new V6Value(V6Value.TAG_OBJ, 0, o);
  }

  private static final V6Value UNDEF = new V6Value(V6Value.TAG_UNDEF, 0, null);

  private static V6Value[] restFrom(V6Value[] args, int from) {
    if (args.length <= from)
      return new V6Value[0];
    V6Value[] out = new V6Value[args.length - from];
    System.arraycopy(args, from, out, 0, out.length);
    return out;
  }

  @Override
  public V6Value construct(V6Value[] args) {
    V6Object self = new V6Object();
    V6Value[] currentStore = {UNDEF};

    self.set("run", fn((t, a) -> {
               V6Value store = V6Value.argAt(a, 0);
               V6Callable callback = V6Value.argAt(a, 1).asCallable();
               V6Value prev = currentStore[0];
               currentStore[0] = store;
               try {
                 return callback.call(UNDEF, restFrom(a, 2));
               } finally {
                 currentStore[0] = prev;
               }
             }));

    self.set("getStore", fn((t, a) -> currentStore[0]));

    self.set("enterWith", fn((t, a) -> {
               currentStore[0] = V6Value.argAt(a, 0);
               return UNDEF;
             }));

    self.set("exit", fn((t, a) -> {
               V6Callable callback = V6Value.argAt(a, 0).asCallable();
               V6Value prev = currentStore[0];
               currentStore[0] = UNDEF;
               try {
                 return callback.call(UNDEF, restFrom(a, 1));
               } finally {
                 currentStore[0] = prev;
               }
             }));

    self.set("disable", fn((t, a) -> {
               currentStore[0] = UNDEF;
               return UNDEF;
             }));

    return objValue(self);
  }
}
