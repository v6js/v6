public final class V6Builtins {
  private static V6Value fn(V6Callable c) {
    return new V6Value(V6Value.TAG_FUNC, 0, c);
  }

  private static V6Value fn1(java.util.function.DoubleUnaryOperator f) {
    return fn(
        (thisArg, args)
            -> new V6Value(V6Value.TAG_NUM,
                           f.applyAsDouble(V6Value.argAt(args, 0).toNumber()),
                           null));
  }

  private static V6Value fn2(java.util.function.DoubleBinaryOperator f) {
    return fn(
        (thisArg, args)
            -> new V6Value(V6Value.TAG_NUM,
                           f.applyAsDouble(V6Value.argAt(args, 0).toNumber(),
                                           V6Value.argAt(args, 1).toNumber()),
                           null));
  }

  private static V6Value boolValue(boolean b) {
    return new V6Value(V6Value.TAG_BOOL, b ? 1 : 0, null);
  }

  private static V6Value objValue(V6Object o) {
    return new V6Value(V6Value.TAG_OBJ, 0, o);
  }

  private static final V6Value UNDEF = new V6Value(V6Value.TAG_UNDEF, 0, null);

  private static V6Object asObj(V6Value v) {
    return v.tag() == V6Value.TAG_OBJ ? (V6Object)v.ref() : null;
  }

  public static final V6Value PRINT = fn((thisArg, args) -> {
    System.out.println(V6Value.argAt(args, 0).toString());
    return UNDEF;
  });

  public static final V6Value ABS = fn1(Math::abs);
  public static final V6Value FLOOR = fn1(Math::floor);
  public static final V6Value CEIL = fn1(Math::ceil);
  public static final V6Value ROUND = fn1(a -> Math.round(a));
  public static final V6Value TRUNC = fn1(a -> (double)(long)a);
  public static final V6Value SQRT = fn1(Math::sqrt);
  public static final V6Value CBRT = fn1(Math::cbrt);
  public static final V6Value SIGN = fn1(Math::signum);
  public static final V6Value MAX = fn2(Math::max);
  public static final V6Value MIN = fn2(Math::min);
  public static final V6Value POW = fn2(Math::pow);
  public static final V6Value RANDOM =
      fn((thisArg, args) -> new V6Value(V6Value.TAG_NUM, Math.random(), null));

  private static V6Object mathObject() {
    V6Object o = new V6Object();
    o.set("abs", ABS);
    o.set("floor", FLOOR);
    o.set("ceil", CEIL);
    o.set("round", ROUND);
    o.set("trunc", TRUNC);
    o.set("sqrt", SQRT);
    o.set("cbrt", CBRT);
    o.set("sign", SIGN);
    o.set("max", MAX);
    o.set("min", MIN);
    o.set("pow", POW);
    o.set("random", RANDOM);
    o.set("PI", new V6Value(V6Value.TAG_NUM, Math.PI, null));
    o.set("E", new V6Value(V6Value.TAG_NUM, Math.E, null));
    return o;
  }

  public static final V6Value MATH = objValue(mathObject());

  public static final V6Object ARRAY_PROTOTYPE = arrayPrototype();

  private static V6Object arrayPrototype() {
    V6Object o = new V6Object();
    o.set("push", fn((thisArg, args) -> {
            V6Object a = asObj(thisArg);
            for (V6Value v : args)
              a.push(v);
            return new V6Value(V6Value.TAG_NUM, a.get("length").num(), null);
          }));
    o.set("pop", fn((thisArg, args) -> asObj(thisArg).pop()));
    o.set("shift", fn((thisArg, args) -> asObj(thisArg).shift()));
    o.set("unshift",
          fn((thisArg, args)
                 -> new V6Value(V6Value.TAG_NUM, asObj(thisArg).unshift(args),
                                null)));
    o.set("slice", fn((thisArg, args) -> {
            V6Object a = asObj(thisArg);
            int len = (int)a.get("length").num();
            int start = args.length > 0 ? (int)args[0].toNumber() : 0;
            int end = args.length > 1 ? (int)args[1].toNumber() : len;
            return objValue(a.slice(start, end));
          }));
    o.set("indexOf",
          fn((thisArg, args)
                 -> new V6Value(V6Value.TAG_NUM,
                                asObj(thisArg).indexOf(V6Value.argAt(args, 0)),
                                null)));
    o.set("includes", fn((thisArg, args)
                             -> boolValue(asObj(thisArg).indexOf(
                                              V6Value.argAt(args, 0)) >= 0)));
    o.set("join", fn((thisArg, args) -> {
            String sep = args.length > 0 ? args[0].toString() : ",";
            return new V6Value(V6Value.TAG_STR, 0, asObj(thisArg).join(sep));
          }));
    o.set("map", fn((thisArg, args)
                        -> objValue(asObj(thisArg).map(
                            V6Value.argAt(args, 0).asCallable()))));
    o.set("filter", fn((thisArg, args)
                           -> objValue(asObj(thisArg).filter(
                               V6Value.argAt(args, 0).asCallable()))));
    o.set("forEach", fn((thisArg, args) -> {
            asObj(thisArg).forEach(V6Value.argAt(args, 0).asCallable());
            return UNDEF;
          }));
    o.set("reduce", fn((thisArg, args)
                           -> asObj(thisArg).reduce(
                               V6Value.argAt(args, 0).asCallable(), args)));
    o.set("concat",
          fn((thisArg, args) -> objValue(asObj(thisArg).concatValues(args))));
    o.set("reverse",
          fn((thisArg,
              args) -> objValue((V6Array)asObj(thisArg).reverseInPlace())));
    o.set("sort", fn((thisArg, args) -> {
            V6Object a = asObj(thisArg);
            if (args.length > 0 && args[0].tag() == V6Value.TAG_FUNC)
              return objValue((V6Array)a.sortWith(args[0].asCallable()));
            return objValue((V6Array)a.sortDefault());
          }));
    return o;
  }

  private static V6Object objectNamespace() {
    V6Object o = new V6Object();
    o.set("keys", fn((thisArg, args) -> {
            V6Object obj = asObj(V6Value.argAt(args, 0));
            return objValue(obj != null ? obj.enumKeys() : new V6Array());
          }));
    o.set("values", fn((thisArg, args) -> {
            V6Object obj = asObj(V6Value.argAt(args, 0));
            V6Array result = new V6Array();
            if (obj != null)
              for (V6Value k : obj.enumKeys().toValueArray())
                result.push(obj.get(k.toString()));
            return objValue(result);
          }));
    o.set("entries", fn((thisArg, args) -> {
            V6Object obj = asObj(V6Value.argAt(args, 0));
            V6Array result = new V6Array();
            if (obj != null) {
              for (V6Value k : obj.enumKeys().toValueArray()) {
                V6Array pair = new V6Array();
                pair.push(k);
                pair.push(obj.get(k.toString()));
                result.push(objValue(pair));
              }
            }
            return objValue(result);
          }));
    o.set("assign", fn((thisArg, args) -> {
            V6Value target = V6Value.argAt(args, 0);
            V6Object t = asObj(target);
            if (t != null)
              for (int i = 1; i < args.length; i++)
                t.spreadFrom(args[i]);
            return target;
          }));
    o.set("freeze", fn((thisArg, args) -> {
            V6Value v = V6Value.argAt(args, 0);
            V6Object obj = asObj(v);
            if (obj != null)
              obj.freeze();
            return v;
          }));
    o.set("isFrozen", fn((thisArg, args) -> {
            V6Object obj = asObj(V6Value.argAt(args, 0));
            return boolValue(obj != null && obj.isFrozenFlag());
          }));
    o.set("seal", fn((thisArg, args) -> {
            V6Value v = V6Value.argAt(args, 0);
            V6Object obj = asObj(v);
            if (obj != null)
              obj.seal();
            return v;
          }));
    o.set("isSealed", fn((thisArg, args) -> {
            V6Object obj = asObj(V6Value.argAt(args, 0));
            return boolValue(obj != null && obj.isSealedFlag());
          }));
    o.set("create", fn((thisArg, args) -> {
            V6Object obj = new V6Object();
            obj.setProtoFromValue(V6Value.argAt(args, 0));
            return objValue(obj);
          }));
    o.set("getPrototypeOf", fn((thisArg, args) -> {
            V6Object obj = asObj(V6Value.argAt(args, 0));
            V6Object proto = obj != null ? obj.getProto() : null;
            return proto != null ? objValue(proto)
                                 : new V6Value(V6Value.TAG_NULL, 0, null);
          }));
    o.set("setPrototypeOf", fn((thisArg, args) -> {
            V6Value v = V6Value.argAt(args, 0);
            V6Object obj = asObj(v);
            if (obj != null)
              obj.setProtoFromValue(V6Value.argAt(args, 1));
            return v;
          }));
    o.set("fromEntries", fn((thisArg, args) -> {
            V6Object entries = asObj(V6Value.argAt(args, 0));
            V6Object result = new V6Object();
            if (entries != null) {
              int n = (int)entries.get("length").num();
              for (int i = 0; i < n; i++) {
                V6Object pair = asObj(entries.get(Integer.toString(i)));
                if (pair != null)
                  result.set(pair.get("0").toString(), pair.get("1"));
              }
            }
            return objValue(result);
          }));
    o.set("is", fn((thisArg, args)
                       -> boolValue(V6Value.strictEquals(
                           V6Value.argAt(args, 0), V6Value.argAt(args, 1)))));
    return o;
  }

  public static final V6Value OBJECT = objValue(objectNamespace());

  private static V6Object arrayNamespace() {
    V6Object o = new V6Object();
    o.set("isArray", fn((thisArg, args)
                            -> boolValue(V6Value.argAt(args, 0).ref() instanceof
                                         V6Array)));
    o.set("from", fn((thisArg, args) -> {
            V6Array result = new V6Array();
            result.pushAll(V6Value.argAt(args, 0));
            if (args.length > 1 && args[1].tag() == V6Value.TAG_FUNC)
              return objValue(result.map(args[1].asCallable()));
            return objValue(result);
          }));
    o.set("of", fn((thisArg, args) -> {
            V6Array result = new V6Array();
            for (V6Value v : args)
              result.push(v);
            return objValue(result);
          }));
    return o;
  }

  public static final V6Value ARRAY = objValue(arrayNamespace());

  public static final V6Value BTOA = fn((thisArg, args) -> {
    String s = V6Value.argAt(args, 0).toString();
    byte[] bytes = new byte[s.length()];
    for (int i = 0; i < s.length(); i++)
      bytes[i] = (byte)s.charAt(i);
    return new V6Value(V6Value.TAG_STR, 0,
                       java.util.Base64.getEncoder().encodeToString(bytes));
  });

  public static final V6Value ATOB = fn((thisArg, args) -> {
    String s = V6Value.argAt(args, 0).toString();
    byte[] bytes = java.util.Base64.getDecoder().decode(s);
    StringBuilder sb = new StringBuilder();
    for (byte b : bytes)
      sb.append((char)(b & 0xFF));
    return new V6Value(V6Value.TAG_STR, 0, sb.toString());
  });
}
