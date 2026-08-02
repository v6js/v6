public final class V6Assert {
  private V6Assert() {}

  private static V6Value str(String s) {
    return new V6Value(V6Value.TAG_STR, 0, s);
  }

  private static V6Value fn(V6Callable c) {
    return new V6Value(V6Value.TAG_FUNC, 0, c);
  }

  private static final V6Value UNDEF = new V6Value(V6Value.TAG_UNDEF, 0, null);

  private static java.util.Set<String> keysOf(V6Object o) {
    java.util.Set<String> keys = new java.util.HashSet<>();
    for (int i = 0; i < o.elemCount; i++)
      keys.add(Integer.toString(i));
    keys.addAll(o.props.keySet());
    return keys;
  }

  private static boolean deepEqual(V6Value a, V6Value b,
                                   java.util.IdentityHashMap<Object, Boolean> seen) {
    if (a.tag() != b.tag()) {
      if ((a.tag() == V6Value.TAG_NULL || a.tag() == V6Value.TAG_UNDEF) &&
          (b.tag() == V6Value.TAG_NULL || b.tag() == V6Value.TAG_UNDEF))
        return true;
      return false;
    }
    switch (a.tag()) {
    case V6Value.TAG_NUM:
      return a.toNumber() == b.toNumber();
    case V6Value.TAG_STR:
      return a.toString().equals(b.toString());
    case V6Value.TAG_BOOL:
      return a.truthy() == b.truthy();
    case V6Value.TAG_NULL:
    case V6Value.TAG_UNDEF:
      return true;
    case V6Value.TAG_OBJ:
      if (a.ref() == b.ref())
        return true;
      if (!(a.ref() instanceof V6Object) || !(b.ref() instanceof V6Object))
        return a.ref() != null && a.ref().equals(b.ref());
      if (seen.containsKey(a.ref()))
        return true;
      seen.put(a.ref(), true);
      V6Object oa = (V6Object)a.ref();
      V6Object ob = (V6Object)b.ref();
      java.util.Set<String> keysA = keysOf(oa);
      java.util.Set<String> keysB = keysOf(ob);
      if (!keysA.equals(keysB))
        return false;
      for (String k : keysA)
        if (!deepEqual(oa.get(k), ob.get(k), seen))
          return false;
      return true;
    default:
      return false;
    }
  }

  private static RuntimeException fail(V6Value[] args, int msgIdx, String defaultMsg) {
    String msg = args.length > msgIdx && !args[msgIdx].isUndefined()
        ? args[msgIdx].toString()
        : defaultMsg;
    return new V6Throw(str(msg));
  }

  public static V6AssertFunction build() {
    V6AssertFunction o = new V6AssertFunction();

    o.set("ok", fn((thisArg, args) -> {
            V6Value v = V6Value.argAt(args, 0);
            if (!v.truthy())
              throw fail(args, 1, "AssertionError: " + v + " is falsy");
            return UNDEF;
          }));

    o.set("equal", fn((thisArg, args) -> {
            V6Value a = V6Value.argAt(args, 0);
            V6Value b = V6Value.argAt(args, 1);
            if (!V6Value.looseEquals(a, b))
              throw fail(args, 2, "AssertionError: " + a + " != " + b);
            return UNDEF;
          }));

    o.set("notEqual", fn((thisArg, args) -> {
            V6Value a = V6Value.argAt(args, 0);
            V6Value b = V6Value.argAt(args, 1);
            if (V6Value.looseEquals(a, b))
              throw fail(args, 2, "AssertionError: " + a + " == " + b);
            return UNDEF;
          }));

    o.set("strictEqual", fn((thisArg, args) -> {
            V6Value a = V6Value.argAt(args, 0);
            V6Value b = V6Value.argAt(args, 1);
            if (!V6Value.strictEquals(a, b))
              throw fail(args, 2, "AssertionError: " + a + " !== " + b);
            return UNDEF;
          }));

    o.set("notStrictEqual", fn((thisArg, args) -> {
            V6Value a = V6Value.argAt(args, 0);
            V6Value b = V6Value.argAt(args, 1);
            if (V6Value.strictEquals(a, b))
              throw fail(args, 2, "AssertionError: " + a + " === " + b);
            return UNDEF;
          }));

    o.set("deepEqual", fn((thisArg, args) -> {
            V6Value a = V6Value.argAt(args, 0);
            V6Value b = V6Value.argAt(args, 1);
            if (!deepEqual(a, b, new java.util.IdentityHashMap<>()))
              throw fail(args, 2, "AssertionError: values are not deep-equal");
            return UNDEF;
          }));
    o.set("deepStrictEqual", o.get("deepEqual"));

    o.set("notDeepEqual", fn((thisArg, args) -> {
            V6Value a = V6Value.argAt(args, 0);
            V6Value b = V6Value.argAt(args, 1);
            if (deepEqual(a, b, new java.util.IdentityHashMap<>()))
              throw fail(args, 2, "AssertionError: values are deep-equal");
            return UNDEF;
          }));
    o.set("notDeepStrictEqual", o.get("notDeepEqual"));

    o.set("throws", fn((thisArg, args) -> {
            V6Callable target = V6Value.argAt(args, 0).asCallable();
            boolean threw = false;
            try {
              target.call(UNDEF, new V6Value[0]);
            } catch (V6Throw e) {
              threw = true;
            } catch (RuntimeException e) {
              threw = true;
            }
            if (!threw)
              throw fail(args, 1, "AssertionError: expected function to throw");
            return UNDEF;
          }));

    o.set("doesNotThrow", fn((thisArg, args) -> {
            V6Callable target = V6Value.argAt(args, 0).asCallable();
            try {
              target.call(UNDEF, new V6Value[0]);
            } catch (V6Throw e) {
              throw fail(args, 1, "AssertionError: expected function not to throw");
            }
            return UNDEF;
          }));

    o.set("fail",
          fn((thisArg, args) -> { throw fail(args, 0, "AssertionError: failed"); }));

    return o;
  }
}
