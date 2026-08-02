public final class V6Assert {
  private V6Assert() {
  }

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

  private static boolean
  deepEqual(V6Value a, V6Value b,
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

  private static V6Value objValue(V6Object o) {
    return new V6Value(V6Value.TAG_OBJ, 0, o);
  }

  private static RuntimeException fail(V6Value[] args, int msgIdx,
                                       String defaultMsg) {
    String msg = args.length > msgIdx && !args[msgIdx].isUndefined()
                     ? args[msgIdx].toString()
                     : defaultMsg;
    return new V6Throw(str(msg));
  }

  private static boolean isConstructorLike(V6Value v) {
    return v.tag() == V6Value.TAG_FUNC ||
        (v.tag() == V6Value.TAG_OBJ &&
         (v.ref() instanceof V6NativeConstructor || v.ref() instanceof
                                                        V6Class));
  }

  private static boolean matchesError(V6Value errVal, V6Value matcher) {
    if (matcher.isUndefined())
      return true;
    try {
      if (V6Value.instanceOf(errVal, matcher))
        return true;
    } catch (RuntimeException ignored) {
    }
    if (matcher.tag() == V6Value.TAG_FUNC) {
      try {
        return matcher.asCallable()
            .call(UNDEF, new V6Value[] {errVal})
            .truthy();
      } catch (RuntimeException e) {
        return false;
      }
    }
    if (isConstructorLike(matcher))
      return false;
    if (matcher.tag() == V6Value.TAG_OBJ && matcher.ref() instanceof V6Regex) {
      String s =
          errVal.tag() == V6Value.TAG_OBJ && errVal.ref() instanceof V6Object
              ? ((V6Object)errVal.ref()).get("message").toString()
              : errVal.toString();
      return ((V6Regex)matcher.ref()).pattern.matcher(s).find();
    }
    if (matcher.tag() == V6Value.TAG_OBJ && matcher.ref() instanceof V6Object) {
      V6Object mo = (V6Object)matcher.ref();
      if (!(errVal.tag() == V6Value.TAG_OBJ && errVal.ref() instanceof
                                                   V6Object))
        return false;
      V6Object eo = (V6Object)errVal.ref();
      for (String k : mo.props.keySet())
        if (!V6Value.looseEquals(mo.get(k), eo.get(k)))
          return false;
      return true;
    }
    return V6Value.strictEquals(errVal, matcher);
  }

  private static V6Value[] extractMatcherAndMessage(V6Value[] args,
                                                    int startIdx) {
    V6Value matcher = UNDEF;
    V6Value message = UNDEF;
    if (args.length > startIdx) {
      V6Value a1 = args[startIdx];
      if (args.length > startIdx + 1) {
        matcher = a1;
        message = args[startIdx + 1];
      } else if (a1.tag() == V6Value.TAG_STR) {
        message = a1;
      } else {
        matcher = a1;
      }
    }
    return new V6Value[] {matcher, message};
  }

  private static V6Value messageOrDefault(V6Value message, String def) {
    if (message.isUndefined())
      return str(def);
    return message.tag() == V6Value.TAG_STR
        ? str("AssertionError: " + message.toString())
        : message;
  }

  private static V6Promise coerceToPromise(V6Value v) {
    if (v.tag() == V6Value.TAG_OBJ && v.ref() instanceof V6Promise)
      return (V6Promise)v.ref();
    if (v.tag() == V6Value.TAG_FUNC) {
      try {
        return coerceToPromise(v.asCallable().call(UNDEF, new V6Value[0]));
      } catch (V6Throw e) {
        return V6Promise.rejected(e.value);
      } catch (RuntimeException e) {
        return V6Promise.rejected(str(String.valueOf(e.getMessage())));
      }
    }
    return V6Promise.resolved(v);
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
              throw fail(args, 1,
                         "AssertionError: expected function not to throw");
            }
            return UNDEF;
          }));

    o.set("fail", fn((thisArg, args) -> {
            throw fail(args, 0, "AssertionError: failed");
          }));

    o.set("match", fn((thisArg, args) -> {
            String s = V6Value.argAt(args, 0).toString();
            V6Value re = V6Value.argAt(args, 1);
            boolean matches = re.tag() == V6Value.TAG_OBJ &&
                              re.ref() instanceof V6Regex &&
                              ((V6Regex)re.ref()).pattern.matcher(s).find();
            if (!matches)
              throw fail(args, 2,
                         "AssertionError: " + s + " does not match " + re);
            return UNDEF;
          }));

    o.set("doesNotMatch", fn((thisArg, args) -> {
            String s = V6Value.argAt(args, 0).toString();
            V6Value re = V6Value.argAt(args, 1);
            boolean matches = re.tag() == V6Value.TAG_OBJ &&
                              re.ref() instanceof V6Regex &&
                              ((V6Regex)re.ref()).pattern.matcher(s).find();
            if (matches)
              throw fail(args, 2,
                         "AssertionError: " + s + " should not match " + re);
            return UNDEF;
          }));

    o.set(
        "rejects", fn((thisArg, args) -> {
          V6Value[] me = extractMatcherAndMessage(args, 1);
          V6Value matcher = me[0];
          V6Value message = me[1];
          V6Promise resultPromise = new V6Promise();
          V6Promise targetPromise = coerceToPromise(V6Value.argAt(args, 0));
          targetPromise.addCallbacks(
              (okVal)
                  -> resultPromise.reject(messageOrDefault(
                      message, "AssertionError: Missing expected rejection.")),
              (errVal) -> {
                if (matchesError(errVal, matcher))
                  resultPromise.resolve(UNDEF);
                else
                  resultPromise.reject(messageOrDefault(
                      message, "AssertionError: rejection did not match " +
                               "expected error."));
              });
          return objValue(resultPromise);
        }));

    o.set("doesNotReject", fn((thisArg, args) -> {
            V6Value[] me = extractMatcherAndMessage(args, 1);
            V6Value message = me[1];
            V6Promise resultPromise = new V6Promise();
            V6Promise targetPromise = coerceToPromise(V6Value.argAt(args, 0));
            targetPromise.addCallbacks(
                (okVal)
                    -> resultPromise.resolve(UNDEF),
                (errVal)
                    -> resultPromise.reject(messageOrDefault(
                        message,
                        "AssertionError: expected promise not to reject.")));
            return objValue(resultPromise);
          }));

    o.set("CallTracker", objValue(new V6CallTrackerConstructor()));

    return o;
  }
}
