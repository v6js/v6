public final class V6ArrayConstructor
    extends V6Object implements V6NativeConstructor {
  public V6ArrayConstructor() {
    set("prototype",
        new V6Value(V6Value.TAG_OBJ, 0, V6Builtins.ARRAY_PROTOTYPE));
    set("isArray",
        new V6Value(V6Value.TAG_FUNC, 0, (V6Callable)(thisArg, args) -> {
          V6Value v = V6Value.argAt(args, 0);
          return new V6Value(V6Value.TAG_BOOL,
                             v.ref() instanceof V6Array ? 1 : 0, null);
        }));
    set("from",
        new V6Value(V6Value.TAG_FUNC, 0, (V6Callable)(thisArg, args) -> {
          V6Array result = new V6Array();
          result.pushAll(V6Value.argAt(args, 0));
          if (args.length > 1 && args[1].tag() == V6Value.TAG_FUNC)
            return new V6Value(V6Value.TAG_OBJ, 0,
                               result.map(args[1].asCallable()));
          return new V6Value(V6Value.TAG_OBJ, 0, result);
        }));
    set("of", new V6Value(V6Value.TAG_FUNC, 0, (V6Callable)(thisArg, args) -> {
          V6Array result = new V6Array();
          for (V6Value v : args)
            result.push(v);
          return new V6Value(V6Value.TAG_OBJ, 0, result);
        }));
  }

  @Override
  public V6Value construct(V6Value[] args) {
    V6Array result = new V6Array();
    if (args.length == 1 && args[0].tag() == V6Value.TAG_NUM) {
      int n = (int)args[0].num();
      for (int i = 0; i < n; i++)
        result.push(new V6Value(V6Value.TAG_UNDEF, 0, null));
    } else {
      for (V6Value v : args)
        result.push(v);
    }
    return new V6Value(V6Value.TAG_OBJ, 0, result);
  }

  @Override
  public V6Object prototypeObject() {
    return V6Builtins.ARRAY_PROTOTYPE;
  }
}
