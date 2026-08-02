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

  private static V6Value str(String s) {
    return new V6Value(V6Value.TAG_STR, 0, s);
  }

  private static final V6Value UNDEF = new V6Value(V6Value.TAG_UNDEF, 0, null);

  private static V6Object asObj(V6Value v) {
    return v.tag() == V6Value.TAG_OBJ ? (V6Object)v.ref() : null;
  }

  private static String inspect(V6Value v, java.util.Map<Object, Boolean> seen) {
    if (v.tag() == V6Value.TAG_STR)
      return "'" + v.toString().replace("'", "\\'") + "'";
    if (v.tag() == V6Value.TAG_BIGINT)
      return v.toString() + "n";
    if (v.tag() != V6Value.TAG_OBJ)
      return v.toString();
    Object ref = v.ref();
    if (seen.containsKey(ref))
      return "[Circular *1]";
    seen.put(ref, Boolean.TRUE);
    String result;
    if (ref instanceof V6Array) {
      V6Array arr = (V6Array)ref;
      int len = (int)arr.get("length").num();
      if (len == 0) {
        result = "[]";
      } else {
        StringBuilder sb = new StringBuilder("[ ");
        for (int i = 0; i < len; i++) {
          if (i > 0)
            sb.append(", ");
          sb.append(inspect(arr.get(Integer.toString(i)), seen));
        }
        sb.append(" ]");
        result = sb.toString();
      }
    } else {
      V6Object obj = (V6Object)ref;
      V6Array keys = obj.enumKeys();
      int n = (int)keys.get("length").num();
      if (n == 0) {
        result = "{}";
      } else {
        StringBuilder sb = new StringBuilder("{ ");
        for (int i = 0; i < n; i++) {
          if (i > 0)
            sb.append(", ");
          String key = keys.get(Integer.toString(i)).toString();
          sb.append(key).append(": ").append(inspect(obj.get(key), seen));
        }
        sb.append(" }");
        result = sb.toString();
      }
    }
    seen.remove(ref);
    return result;
  }

  private static String inspectTop(V6Value v) {
    if (v.tag() == V6Value.TAG_STR)
      return v.toString();
    return inspect(v, new java.util.IdentityHashMap<>());
  }

  public static final V6Value CONSOLE_LOG = fn((thisArg, args) -> {
    StringBuilder sb = new StringBuilder();
    for (int i = 0; i < args.length; i++) {
      if (i > 0)
        sb.append(' ');
      sb.append(inspectTop(args[i]));
    }
    System.out.println(sb.toString());
    return UNDEF;
  });

  private static V6Object consoleObject() {
    V6Object o = new V6Object();
    o.set("log", CONSOLE_LOG);
    o.set("error", CONSOLE_LOG);
    o.set("warn", CONSOLE_LOG);
    o.set("info", CONSOLE_LOG);
    return o;
  }

  public static final V6Value CONSOLE = objValue(consoleObject());

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
    o.set("defineProperty", fn((thisArg, args) -> {
            V6Value targetV = V6Value.argAt(args, 0);
            V6Object target = asObj(targetV);
            String key = V6Value.argAt(args, 1).toString();
            V6Object desc = asObj(V6Value.argAt(args, 2));
            applyDescriptor(target, key, desc);
            return targetV;
          }));
    o.set("defineProperties", fn((thisArg, args) -> {
            V6Value targetV = V6Value.argAt(args, 0);
            V6Object target = asObj(targetV);
            V6Object descs = asObj(V6Value.argAt(args, 1));
            if (target != null && descs != null) {
              for (V6Value k : descs.enumKeys().toValueArray()) {
                String key = k.toString();
                applyDescriptor(target, key, asObj(descs.get(key)));
              }
            }
            return targetV;
          }));
    return o;
  }

  private static void applyDescriptor(V6Object target, String key, V6Object desc) {
    if (target == null || desc == null)
      return;
    if (desc.has("get") || desc.has("set")) {
      V6Value g = desc.get("get");
      V6Value st = desc.get("set");
      if (g.tag() == V6Value.TAG_FUNC)
        target.defineGetter(key, g.asCallable());
      if (st.tag() == V6Value.TAG_FUNC)
        target.defineSetter(key, st.asCallable());
    } else {
      target.set(key, desc.get("value"));
    }
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

  public static final V6Value PARSE_INT = fn((thisArg, args) -> {
    String str = V6Value.argAt(args, 0).toString().strip();
    int radix = args.length > 1 && !args[1].isUndefined()
                    ? (int)args[1].toNumber()
                    : 0;
    int sign = 1;
    int i = 0;
    int n = str.length();
    if (i < n && (str.charAt(i) == '+' || str.charAt(i) == '-')) {
      if (str.charAt(i) == '-')
        sign = -1;
      i++;
    }
    if ((radix == 16 || radix == 0) && i + 1 < n && str.charAt(i) == '0' &&
        (str.charAt(i + 1) == 'x' || str.charAt(i + 1) == 'X')) {
      radix = 16;
      i += 2;
    } else if (radix == 0) {
      radix = 10;
    }
    int start = i;
    while (i < n && Character.digit(str.charAt(i), radix) >= 0)
      i++;
    if (i == start)
      return new V6Value(V6Value.TAG_NUM, Double.NaN, null);
    try {
      return new V6Value(
          V6Value.TAG_NUM,
          sign * (double)Long.parseLong(str.substring(start, i), radix), null);
    } catch (NumberFormatException e) {
      return new V6Value(V6Value.TAG_NUM, Double.NaN, null);
    }
  });

  public static final V6Value BIGINT = fn((thisArg, args) -> {
    V6Value v = V6Value.argAt(args, 0);
    if (v.tag() == V6Value.TAG_BIGINT)
      return v;
    if (v.tag() == V6Value.TAG_STR)
      return V6Value.bigint(new java.math.BigInteger(v.toString().strip()));
    double n = v.toNumber();
    if (n != Math.rint(n) || Double.isNaN(n) || Double.isInfinite(n))
      throw new RuntimeException(
          "Cannot convert non-integer to BigInt: " + n);
    return V6Value.bigint(
        new java.math.BigDecimal(n).toBigInteger());
  });

  public static final V6Value PARSE_FLOAT = fn((thisArg, args) -> {
    String str = V6Value.argAt(args, 0).toString().strip();
    int i = 0;
    int n = str.length();
    if (i < n && (str.charAt(i) == '+' || str.charAt(i) == '-'))
      i++;
    int start0 = i;
    while (i < n && Character.isDigit(str.charAt(i)))
      i++;
    if (i < n && str.charAt(i) == '.') {
      i++;
      while (i < n && Character.isDigit(str.charAt(i)))
        i++;
    }
    if (i < n && (str.charAt(i) == 'e' || str.charAt(i) == 'E')) {
      int save = i;
      i++;
      if (i < n && (str.charAt(i) == '+' || str.charAt(i) == '-'))
        i++;
      if (i < n && Character.isDigit(str.charAt(i))) {
        while (i < n && Character.isDigit(str.charAt(i)))
          i++;
      } else {
        i = save;
      }
    }
    if (i == start0 || (i == start0 + 1 && str.charAt(start0) == '.'))
      return new V6Value(V6Value.TAG_NUM, Double.NaN, null);
    try {
      return new V6Value(V6Value.TAG_NUM, Double.parseDouble(str.substring(0, i)),
                         null);
    } catch (NumberFormatException e) {
      return new V6Value(V6Value.TAG_NUM, Double.NaN, null);
    }
  });

  public static final V6Value IS_NAN = fn(
      (thisArg, args) -> boolValue(Double.isNaN(V6Value.argAt(args, 0).toNumber())));

  public static final V6Value IS_FINITE = fn((thisArg, args) -> {
    double n = V6Value.argAt(args, 0).toNumber();
    return boolValue(!Double.isNaN(n) && !Double.isInfinite(n));
  });

  private static V6Object numberNamespace() {
    V6Object o = new V6Object();
    o.set("isInteger", fn((thisArg, args) -> {
            V6Value v = V6Value.argAt(args, 0);
            if (v.tag() != V6Value.TAG_NUM)
              return boolValue(false);
            double n = v.toNumber();
            return boolValue(!Double.isNaN(n) && !Double.isInfinite(n) &&
                             n == Math.floor(n));
          }));
    o.set("isFinite", fn((thisArg, args) -> {
            V6Value v = V6Value.argAt(args, 0);
            if (v.tag() != V6Value.TAG_NUM)
              return boolValue(false);
            double n = v.toNumber();
            return boolValue(!Double.isNaN(n) && !Double.isInfinite(n));
          }));
    o.set("isNaN", fn((thisArg, args) -> {
            V6Value v = V6Value.argAt(args, 0);
            return boolValue(v.tag() == V6Value.TAG_NUM &&
                             Double.isNaN(v.toNumber()));
          }));
    o.set("isSafeInteger", fn((thisArg, args) -> {
            V6Value v = V6Value.argAt(args, 0);
            if (v.tag() != V6Value.TAG_NUM)
              return boolValue(false);
            double n = v.toNumber();
            return boolValue(!Double.isNaN(n) && !Double.isInfinite(n) &&
                             n == Math.floor(n) &&
                             Math.abs(n) <= 9007199254740991.0);
          }));
    o.set("parseFloat", PARSE_FLOAT);
    o.set("parseInt", PARSE_INT);
    o.set("EPSILON", new V6Value(V6Value.TAG_NUM, Math.ulp(1.0), null));
    o.set("MAX_SAFE_INTEGER",
          new V6Value(V6Value.TAG_NUM, 9007199254740991.0, null));
    o.set("MIN_SAFE_INTEGER",
          new V6Value(V6Value.TAG_NUM, -9007199254740991.0, null));
    o.set("MAX_VALUE", new V6Value(V6Value.TAG_NUM, Double.MAX_VALUE, null));
    o.set("MIN_VALUE", new V6Value(V6Value.TAG_NUM, Double.MIN_VALUE, null));
    o.set("POSITIVE_INFINITY",
          new V6Value(V6Value.TAG_NUM, Double.POSITIVE_INFINITY, null));
    o.set("NEGATIVE_INFINITY",
          new V6Value(V6Value.TAG_NUM, Double.NEGATIVE_INFINITY, null));
    o.set("NaN", new V6Value(V6Value.TAG_NUM, Double.NaN, null));
    return o;
  }

  public static final V6Value NUMBER = objValue(numberNamespace());
  public static final V6Value NAN_VALUE = new V6Value(V6Value.TAG_NUM, Double.NaN, null);
  public static final V6Value INFINITY_VALUE =
      new V6Value(V6Value.TAG_NUM, Double.POSITIVE_INFINITY, null);

  private static V6Object mapPrototype() {
    V6Object o = new V6Object();
    o.set("get", fn((thisArg, args) -> {
            V6MapObject m = (V6MapObject)thisArg.ref();
            V6Value v = m.entries.get(V6MapObject.keyFor(V6Value.argAt(args, 0)));
            return v != null ? v : UNDEF;
          }));
    o.set("set", fn((thisArg, args) -> {
            V6MapObject m = (V6MapObject)thisArg.ref();
            m.entries.put(V6MapObject.keyFor(V6Value.argAt(args, 0)),
                         V6Value.argAt(args, 1));
            return thisArg;
          }));
    o.set("has", fn((thisArg, args)
                        -> boolValue(((V6MapObject)thisArg.ref())
                                         .entries.containsKey(V6MapObject.keyFor(
                                             V6Value.argAt(args, 0))))));
    o.set("delete", fn((thisArg, args) -> {
            V6MapObject m = (V6MapObject)thisArg.ref();
            Object k = V6MapObject.keyFor(V6Value.argAt(args, 0));
            boolean had = m.entries.containsKey(k);
            m.entries.remove(k);
            return boolValue(had);
          }));
    o.set("clear", fn((thisArg, args) -> {
            ((V6MapObject)thisArg.ref()).entries.clear();
            return UNDEF;
          }));
    o.set("forEach", fn((thisArg, args) -> {
            V6MapObject m = (V6MapObject)thisArg.ref();
            V6Callable cb = V6Value.argAt(args, 0).asCallable();
            for (java.util.Map.Entry<Object, V6Value> e : m.entries.entrySet())
              cb.call(UNDEF, new V6Value[] {e.getValue(),
                                            V6MapObject.keyToValue(e.getKey()),
                                            thisArg});
            return UNDEF;
          }));
    o.set("keys", fn((thisArg, args) -> {
            V6MapObject m = (V6MapObject)thisArg.ref();
            V6Array result = new V6Array();
            for (Object k : m.entries.keySet())
              result.push(V6MapObject.keyToValue(k));
            return objValue(result);
          }));
    o.set("values", fn((thisArg, args) -> {
            V6MapObject m = (V6MapObject)thisArg.ref();
            V6Array result = new V6Array();
            for (V6Value v : m.entries.values())
              result.push(v);
            return objValue(result);
          }));
    o.set("entries", fn((thisArg, args) -> {
            V6MapObject m = (V6MapObject)thisArg.ref();
            V6Array result = new V6Array();
            for (java.util.Map.Entry<Object, V6Value> e : m.entries.entrySet()) {
              V6Array pair = new V6Array();
              pair.push(V6MapObject.keyToValue(e.getKey()));
              pair.push(e.getValue());
              result.push(objValue(pair));
            }
            return objValue(result);
          }));
    o.defineGetter(
        "size",
        (thisArg, args) -> num(((V6MapObject)thisArg.ref()).entries.size()));
    return o;
  }

  public static final V6Object MAP_PROTOTYPE = mapPrototype();
  public static final V6Value MAP = objValue(new V6MapConstructor());
  public static final V6Value WEAK_MAP = objValue(new V6MapConstructor());

  private static V6Object setPrototype() {
    V6Object o = new V6Object();
    o.set("add", fn((thisArg, args) -> {
            V6SetObject st = (V6SetObject)thisArg.ref();
            V6Value v = V6Value.argAt(args, 0);
            st.entries.put(V6MapObject.keyFor(v), v);
            return thisArg;
          }));
    o.set("has", fn((thisArg, args)
                        -> boolValue(((V6SetObject)thisArg.ref())
                                         .entries.containsKey(V6MapObject.keyFor(
                                             V6Value.argAt(args, 0))))));
    o.set("delete", fn((thisArg, args) -> {
            V6SetObject st = (V6SetObject)thisArg.ref();
            Object k = V6MapObject.keyFor(V6Value.argAt(args, 0));
            boolean had = st.entries.containsKey(k);
            st.entries.remove(k);
            return boolValue(had);
          }));
    o.set("clear", fn((thisArg, args) -> {
            ((V6SetObject)thisArg.ref()).entries.clear();
            return UNDEF;
          }));
    o.set("forEach", fn((thisArg, args) -> {
            V6SetObject st = (V6SetObject)thisArg.ref();
            V6Callable cb = V6Value.argAt(args, 0).asCallable();
            for (V6Value v : st.entries.values())
              cb.call(UNDEF, new V6Value[] {v, v, thisArg});
            return UNDEF;
          }));
    V6Value valuesFn = fn((thisArg, args) -> {
      V6SetObject st = (V6SetObject)thisArg.ref();
      V6Array result = new V6Array();
      for (V6Value v : st.entries.values())
        result.push(v);
      return objValue(result);
    });
    o.set("values", valuesFn);
    o.set("keys", valuesFn);
    o.set("entries", fn((thisArg, args) -> {
            V6SetObject st = (V6SetObject)thisArg.ref();
            V6Array result = new V6Array();
            for (V6Value v : st.entries.values()) {
              V6Array pair = new V6Array();
              pair.push(v);
              pair.push(v);
              result.push(objValue(pair));
            }
            return objValue(result);
          }));
    o.defineGetter(
        "size",
        (thisArg, args) -> num(((V6SetObject)thisArg.ref()).entries.size()));
    return o;
  }

  public static final V6Object SET_PROTOTYPE = setPrototype();
  public static final V6Value SET = objValue(new V6SetConstructor());
  public static final V6Value WEAK_SET = objValue(new V6SetConstructor());

  private static V6Value num(double n) {
    return new V6Value(V6Value.TAG_NUM, n, null);
  }

  private static V6SymbolFunction symbolFunction() {
    V6SymbolFunction f = new V6SymbolFunction();
    f.set("iterator",
          new V6Value(V6Value.TAG_OBJ, 0, new V6Symbol("Symbol.iterator")));
    java.util.Map<String, V6Symbol> registry = new java.util.HashMap<>();
    f.set("for", fn((thisArg, args) -> {
            String key = V6Value.argAt(args, 0).toString();
            V6Symbol sym = registry.computeIfAbsent(key, V6Symbol::new);
            return new V6Value(V6Value.TAG_OBJ, 0, sym);
          }));
    return f;
  }

  public static final V6Value SYMBOL =
      new V6Value(V6Value.TAG_FUNC, 0, symbolFunction());

  private static V6Object generatorPrototype() {
    V6Object o = new V6Object();
    o.set("next", fn((thisArg, args)
                         -> ((V6Generator)thisArg.ref())
                                .next(V6Value.argAt(args, 0))));
    o.set("return", fn((thisArg, args)
                           -> ((V6Generator)thisArg.ref())
                                  .returnValue(V6Value.argAt(args, 0))));
    o.set("throw", fn((thisArg, args)
                          -> ((V6Generator)thisArg.ref())
                                 .throwInto(V6Value.argAt(args, 0))));
    return o;
  }

  public static final V6Object GENERATOR_PROTOTYPE = generatorPrototype();

  private static V6Object asyncGeneratorPrototype() {
    V6Object o = new V6Object();
    o.set("next", fn((thisArg, args)
                         -> ((V6AsyncGenerator)thisArg.ref())
                                .next(V6Value.argAt(args, 0))));
    o.set("return", fn((thisArg, args)
                           -> ((V6AsyncGenerator)thisArg.ref())
                                  .returnValue(V6Value.argAt(args, 0))));
    o.set("throw", fn((thisArg, args)
                          -> ((V6AsyncGenerator)thisArg.ref())
                                 .throwInto(V6Value.argAt(args, 0))));
    return o;
  }

  public static final V6Object ASYNC_GENERATOR_PROTOTYPE =
      asyncGeneratorPrototype();

  private static final V6Value UNDEF_FN_MARKER =
      new V6Value(V6Value.TAG_UNDEF, 0, null);

  private static V6Object promisePrototype() {
    V6Object o = new V6Object();
    o.set("then", fn((thisArg, args)
                         -> ((V6Promise)thisArg.ref())
                                .then(V6Value.argAt(args, 0),
                                     V6Value.argAt(args, 1))));
    o.set("catch", fn((thisArg, args)
                          -> ((V6Promise)thisArg.ref())
                                 .then(UNDEF_FN_MARKER, V6Value.argAt(args, 0))));
    o.set("finally", fn((thisArg, args) -> {
            V6Value cb = V6Value.argAt(args, 0);
            V6Value onOk = fn((t, a) -> {
              if (cb.tag() == V6Value.TAG_FUNC)
                cb.asCallable().call(UNDEF, new V6Value[0]);
              return V6Value.argAt(a, 0);
            });
            V6Value onErr = fn((t, a) -> {
              if (cb.tag() == V6Value.TAG_FUNC)
                cb.asCallable().call(UNDEF, new V6Value[0]);
              throw new V6Throw(V6Value.argAt(a, 0));
            });
            return ((V6Promise)thisArg.ref()).then(onOk, onErr);
          }));
    return o;
  }

  public static final V6Object PROMISE_PROTOTYPE = promisePrototype();
  public static final V6Value PROMISE = objValue(new V6PromiseConstructor());

  private static V6Array execResult(java.util.regex.Matcher m, String input) {
    V6Array result = new V6Array();
    for (int i = 0; i <= m.groupCount(); i++) {
      String g = m.group(i);
      result.push(g == null ? UNDEF : str(g));
    }
    result.set("index", num(m.start()));
    result.set("input", str(input));
    return result;
  }

  private static V6Object regexPrototype() {
    V6Object o = new V6Object();
    o.set("test", fn((thisArg, args) -> {
            V6Regex re = (V6Regex)thisArg.ref();
            String input = V6Value.argAt(args, 0).toString();
            int start = (re.global || re.sticky)
                ? (int)re.get("lastIndex").toNumber()
                : 0;
            if (start < 0 || start > input.length()) {
              if (re.global || re.sticky)
                re.set("lastIndex", num(0));
              return boolValue(false);
            }
            java.util.regex.Matcher m = re.pattern.matcher(input);
            boolean found = re.sticky ? m.region(start, input.length()).lookingAt()
                                      : m.find(start);
            if (re.global || re.sticky)
              re.set("lastIndex", num(found ? m.end() : 0));
            return boolValue(found);
          }));
    o.set("exec", fn((thisArg, args) -> {
            V6Regex re = (V6Regex)thisArg.ref();
            String input = V6Value.argAt(args, 0).toString();
            int start = (re.global || re.sticky)
                ? (int)re.get("lastIndex").toNumber()
                : 0;
            if (start < 0 || start > input.length()) {
              if (re.global || re.sticky)
                re.set("lastIndex", num(0));
              return V6Value.NUL;
            }
            java.util.regex.Matcher m = re.pattern.matcher(input);
            boolean found = re.sticky ? m.region(start, input.length()).lookingAt()
                                      : m.find(start);
            if (!found) {
              if (re.global || re.sticky)
                re.set("lastIndex", num(0));
              return V6Value.NUL;
            }
            if (re.global || re.sticky)
              re.set("lastIndex", num(m.end()));
            return objValue(execResult(m, input));
          }));
    o.set("toString", fn((thisArg, args) -> {
            V6Object re = (V6Object)thisArg.ref();
            return str("/" + re.get("source").toString() + "/" +
                       re.get("flags").toString());
          }));
    return o;
  }

  public static final V6Object REGEXP_PROTOTYPE = regexPrototype();
  public static final V6Value REGEXP = objValue(new V6RegexConstructor());

  private static V6Object jsonObject() {
    V6Object o = new V6Object();
    o.set("parse", fn((thisArg, args) -> {
            String text = V6Value.argAt(args, 0).toString();
            V6Value result = V6Json.parse(text);
            V6Value reviver = V6Value.argAt(args, 1);
            if (reviver.tag() != V6Value.TAG_FUNC)
              return result;
            V6Object holder = new V6Object();
            holder.set("", result);
            return applyReviver(holder, "", reviver.asCallable());
          }));
    o.set("stringify", fn((thisArg, args) -> {
            V6Value value = V6Value.argAt(args, 0);
            V6Value replacer = V6Value.argAt(args, 1);
            V6Value space = V6Value.argAt(args, 2);
            return V6Json.stringify(value, replacer, space);
          }));
    return o;
  }

  private static V6Value applyReviver(V6Object holder, String key, V6Callable reviver) {
    V6Value value = holder.get(key);
    if (value.tag() == V6Value.TAG_OBJ && value.ref() instanceof V6Object) {
      V6Object obj = (V6Object)value.ref();
      if (obj instanceof V6Array) {
        V6Array arr = (V6Array)obj;
        int n = (int)arr.get("length").num();
        for (int i = 0; i < n; i++) {
          String k = Integer.toString(i);
          V6Value newElem = applyReviver(arr, k, reviver);
          if (newElem.isUndefined())
            arr.set(k, UNDEF);
          else
            arr.set(k, newElem);
        }
      } else {
        V6Array keys = obj.enumKeys();
        int n = (int)keys.get("length").num();
        for (int i = 0; i < n; i++) {
          String k = keys.get(Integer.toString(i)).toString();
          V6Value newElem = applyReviver(obj, k, reviver);
          if (newElem.isUndefined())
            obj.set(k, UNDEF);
          else
            obj.set(k, newElem);
        }
      }
    }
    return reviver.call(objValue(holder),
                        new V6Value[] {new V6Value(V6Value.TAG_STR, 0, key), value});
  }

  public static final V6Value JSON = objValue(jsonObject());

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
