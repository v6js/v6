public record V6Value(int tag, double num, Object ref) {
  public static final int TAG_NUM = 0;
  public static final int TAG_BOOL = 1;
  public static final int TAG_NULL = 2;
  public static final int TAG_UNDEF = 3;
  public static final int TAG_OBJ = 4;
  public static final int TAG_STR = 5;
  public static final int TAG_FUNC = 6;
  public static final int TAG_BIGINT = 7;

  public static final V6Value UNDEF = new V6Value(TAG_UNDEF, 0, null);
  public static final V6Value NUL = new V6Value(TAG_NULL, 0, null);
  public static final V6Value TRUE = new V6Value(TAG_BOOL, 1, null);
  public static final V6Value FALSE = new V6Value(TAG_BOOL, 0, null);

  @Override
  public String toString() {
    return switch (tag) {
      case TAG_NUM -> numToString(num);
      case TAG_BOOL -> num != 0 ? "true" : "false";
      case TAG_NULL -> "null";
      case TAG_UNDEF -> "undefined";
      case TAG_STR -> ref.toString();
      case TAG_FUNC -> "function () { [v6 code] }";
      case TAG_BIGINT -> ref.toString();
      default -> String.valueOf(ref);
    };
  }

  public java.math.BigInteger asBigInt() {
    return (java.math.BigInteger)ref;
  }

  public static V6Value bigint(java.math.BigInteger v) {
    return new V6Value(TAG_BIGINT, 0, v);
  }

  private static RuntimeException mixedBigIntError() {
    return new RuntimeException(
        "Cannot mix BigInt and other types, use explicit conversions");
  }

  private static String numToString(double n) {
    if (Double.isNaN(n))
      return "NaN";
    if (Double.isInfinite(n))
      return n > 0 ? "Infinity" : "-Infinity";
    if (n == 0)
      return "0";
    double abs = Math.abs(n);
    if (n == Math.rint(n) && abs < 1e21)
      return new java.math.BigDecimal(n).toBigInteger().toString();
    if (abs >= 1e21 || abs < 1e-6)
      return Double.toString(n);
    return new java.math.BigDecimal(Double.toString(n)).toPlainString();
  }

  public boolean isNullish() {
    return tag == TAG_NULL || tag == TAG_UNDEF;
  }

  public boolean truthy() {
    return switch (tag) {
      case TAG_NUM -> num != 0 && !Double.isNaN(num);
      case TAG_BOOL -> num != 0;
      case TAG_NULL, TAG_UNDEF -> false;
      case TAG_STR -> ((CharSequence)ref).length() != 0;
      case TAG_BIGINT -> asBigInt().signum() != 0;
      default -> true;
    };
  }

  public double toNumber() {
    return switch (tag) {
      case TAG_NUM, TAG_BOOL -> num;
      case TAG_NULL -> 0;
      case TAG_UNDEF -> Double.NaN;
      case TAG_STR -> parseNumericString(ref.toString());
      case TAG_BIGINT -> asBigInt().doubleValue();
      default -> Double.NaN;
    };
  }

  private static double parseNumericString(String s) {
    String t = s.strip();
    if (t.isEmpty())
      return 0;
    if (t.equals("Infinity") || t.equals("+Infinity"))
      return Double.POSITIVE_INFINITY;
    if (t.equals("-Infinity"))
      return Double.NEGATIVE_INFINITY;
    try {
      if (t.startsWith("0x") || t.startsWith("0X"))
        return Long.parseLong(t.substring(2), 16);
      if (t.startsWith("0o") || t.startsWith("0O"))
        return Long.parseLong(t.substring(2), 8);
      if (t.startsWith("0b") || t.startsWith("0B"))
        return Long.parseLong(t.substring(2), 2);
      return Double.parseDouble(t);
    } catch (NumberFormatException e) {
      return Double.NaN;
    }
  }

  private CharSequence asCharSeq() {
    return tag == TAG_STR ? (CharSequence)ref : toString();
  }

  public static V6Value add(V6Value a, V6Value b) {
    if (a.tag == TAG_STR || b.tag == TAG_STR)
      return new V6Value(TAG_STR, 0, new V6Rope(a.asCharSeq(), b.asCharSeq()));
    if (a.tag == TAG_BIGINT || b.tag == TAG_BIGINT) {
      if (a.tag != TAG_BIGINT || b.tag != TAG_BIGINT)
        throw mixedBigIntError();
      return bigint(a.asBigInt().add(b.asBigInt()));
    }
    return new V6Value(TAG_NUM, a.toNumber() + b.toNumber(), null);
  }

  public static V6Value sub(V6Value a, V6Value b) {
    if (a.tag == TAG_BIGINT || b.tag == TAG_BIGINT) {
      if (a.tag != TAG_BIGINT || b.tag != TAG_BIGINT)
        throw mixedBigIntError();
      return bigint(a.asBigInt().subtract(b.asBigInt()));
    }
    return new V6Value(TAG_NUM, a.toNumber() - b.toNumber(), null);
  }

  public static V6Value mul(V6Value a, V6Value b) {
    if (a.tag == TAG_BIGINT || b.tag == TAG_BIGINT) {
      if (a.tag != TAG_BIGINT || b.tag != TAG_BIGINT)
        throw mixedBigIntError();
      return bigint(a.asBigInt().multiply(b.asBigInt()));
    }
    return new V6Value(TAG_NUM, a.toNumber() * b.toNumber(), null);
  }

  public static V6Value div(V6Value a, V6Value b) {
    if (a.tag == TAG_BIGINT || b.tag == TAG_BIGINT) {
      if (a.tag != TAG_BIGINT || b.tag != TAG_BIGINT)
        throw mixedBigIntError();
      return bigint(a.asBigInt().divide(b.asBigInt()));
    }
    return new V6Value(TAG_NUM, a.toNumber() / b.toNumber(), null);
  }

  public static V6Value mod(V6Value a, V6Value b) {
    if (a.tag == TAG_BIGINT || b.tag == TAG_BIGINT) {
      if (a.tag != TAG_BIGINT || b.tag != TAG_BIGINT)
        throw mixedBigIntError();
      return bigint(a.asBigInt().remainder(b.asBigInt()));
    }
    return new V6Value(TAG_NUM, a.toNumber() % b.toNumber(), null);
  }

  public static V6Value neg(V6Value a) {
    if (a.tag == TAG_BIGINT)
      return bigint(a.asBigInt().negate());
    return new V6Value(TAG_NUM, -a.toNumber(), null);
  }

  public static boolean lt(V6Value a, V6Value b) {
    if (a.tag == TAG_STR && b.tag == TAG_STR)
      return a.ref.toString().compareTo(b.ref.toString()) < 0;
    if (a.tag == TAG_BIGINT && b.tag == TAG_BIGINT)
      return a.asBigInt().compareTo(b.asBigInt()) < 0;
    return a.toNumber() < b.toNumber();
  }

  public static boolean le(V6Value a, V6Value b) {
    if (a.tag == TAG_STR && b.tag == TAG_STR)
      return a.ref.toString().compareTo(b.ref.toString()) <= 0;
    if (a.tag == TAG_BIGINT && b.tag == TAG_BIGINT)
      return a.asBigInt().compareTo(b.asBigInt()) <= 0;
    return a.toNumber() <= b.toNumber();
  }

  public static boolean gt(V6Value a, V6Value b) {
    if (a.tag == TAG_STR && b.tag == TAG_STR)
      return a.ref.toString().compareTo(b.ref.toString()) > 0;
    if (a.tag == TAG_BIGINT && b.tag == TAG_BIGINT)
      return a.asBigInt().compareTo(b.asBigInt()) > 0;
    return a.toNumber() > b.toNumber();
  }

  public static boolean ge(V6Value a, V6Value b) {
    if (a.tag == TAG_STR && b.tag == TAG_STR)
      return a.ref.toString().compareTo(b.ref.toString()) >= 0;
    if (a.tag == TAG_BIGINT && b.tag == TAG_BIGINT)
      return a.asBigInt().compareTo(b.asBigInt()) >= 0;
    return a.toNumber() >= b.toNumber();
  }

  public static boolean strictEquals(V6Value a, V6Value b) {
    if (a.tag != b.tag)
      return false;
    return switch (a.tag) {
      case TAG_NUM, TAG_BOOL -> a.num == b.num;
      case TAG_STR -> a.ref.toString().equals(b.ref.toString());
      case TAG_NULL, TAG_UNDEF -> true;
      case TAG_BIGINT -> a.asBigInt().equals(b.asBigInt());
      default -> a.ref == b.ref;
    };
  }

  public static boolean looseEquals(V6Value a, V6Value b) {
    if (a.tag == b.tag)
      return strictEquals(a, b);
    boolean aNullish = a.tag == TAG_NULL || a.tag == TAG_UNDEF;
    boolean bNullish = b.tag == TAG_NULL || b.tag == TAG_UNDEF;
    if (aNullish || bNullish)
      return aNullish && bNullish;
    if (a.tag == TAG_BOOL)
      return looseEquals(new V6Value(TAG_NUM, a.num, null), b);
    if (b.tag == TAG_BOOL)
      return looseEquals(a, new V6Value(TAG_NUM, b.num, null));
    if (a.tag == TAG_BIGINT || b.tag == TAG_BIGINT)
      return a.toNumber() == b.toNumber();
    return a.toNumber() == b.toNumber();
  }

  public String typeOf() {
    return switch (tag) {
      case TAG_NUM -> "number";
      case TAG_BOOL -> "boolean";
      case TAG_STR -> "string";
      case TAG_UNDEF -> "undefined";
      case TAG_FUNC -> "function";
      case TAG_BIGINT -> "bigint";
      case TAG_OBJ -> ref instanceof V6Symbol ? "symbol" : "object";
      default -> "object";
    };
  }

  public V6Value call(V6Value thisArg, V6Value[] args) {
    if (tag != TAG_FUNC)
      throw new RuntimeException("not a function");
    return ((V6Callable)ref).call(thisArg, args);
  }

  public static V6Value argAt(V6Value[] args, int idx) {
    return idx < args.length ? args[idx] : new V6Value(TAG_UNDEF, 0, null);
  }

  public static V6Value construct(V6Value classValue, V6Value[] args) {
    if (classValue.ref instanceof V6NativeConstructor)
      return ((V6NativeConstructor)classValue.ref).construct(args);
    V6Class cls = (V6Class)classValue.ref;
    V6Object instance = new V6Object();
    Object protoRef = cls.get("prototype").ref;
    if (protoRef instanceof V6Object)
      instance.setProto((V6Object)protoRef);
    V6Value instanceValue = new V6Value(TAG_OBJ, 0, instance);
    instance.newTarget = classValue;
    if (cls.ctor != null)
      cls.ctor.call(instanceValue, args);
    return instanceValue;
  }

  public static void superConstruct(V6Value classValue, V6Value thisArg,
                                    V6Value[] args) {
    V6Class cls = (V6Class)classValue.ref;
    if (cls.ctor != null)
      cls.ctor.call(thisArg, args);
  }

  public V6Callable asCallable() {
    return (V6Callable)ref;
  }

  public boolean isUndefined() {
    return tag == TAG_UNDEF;
  }

  public V6Array restFrom(int start) {
    if (tag == TAG_OBJ)
      return ((V6Object)ref).restFrom(start);
    return new V6Array();
  }

  public V6Value getProp(String key) {
    switch (tag) {
    case TAG_OBJ:
      if (ref instanceof V6Symbol) {
        V6Symbol sym = (V6Symbol)ref;
        if (key.equals("description"))
          return sym.description != null
                     ? new V6Value(TAG_STR, 0, sym.description)
                     : new V6Value(TAG_UNDEF, 0, null);
        if (key.equals("toString"))
          return new V6Value(TAG_FUNC, 0,
                             (V6Callable)(t, a) -> new V6Value(TAG_STR, 0, t.toString()));
        return new V6Value(TAG_UNDEF, 0, null);
      }
      return ((V6Object)ref).get(key);
    case TAG_STR:
      CharSequence s = (CharSequence)ref;
      if (key.equals("length"))
        return new V6Value(TAG_NUM, s.length(), null);
      int idx = V6Object.parseIndex(key);
      if (idx >= 0)
        return idx < s.length()
            ? new V6Value(TAG_STR, 0, String.valueOf(s.charAt(idx)))
            : new V6Value(TAG_UNDEF, 0, null);
      return V6String.PROTOTYPE.get(key);
    case TAG_NUM:
      return V6Number.PROTOTYPE.get(key);
    case TAG_BOOL:
      return V6Boolean.PROTOTYPE.get(key);
    case TAG_FUNC:
      if (ref instanceof V6Object)
        return ((V6Object)ref).get(key);
      return new V6Value(TAG_UNDEF, 0, null);
    default:
      return new V6Value(TAG_UNDEF, 0, null);
    }
  }

  public V6Value enumKeys() {
    if (tag != TAG_OBJ)
      return new V6Value(TAG_OBJ, 0, new V6Array());
    return new V6Value(TAG_OBJ, 0, ((V6Object)ref).enumKeys());
  }

  public void setProp(String key, V6Value value) {
    if (tag == TAG_OBJ)
      ((V6Object)ref).set(key, value);
  }

  public static int toInt32(double d) {
    if (Double.isNaN(d) || Double.isInfinite(d) || d == 0)
      return 0;
    double posInt = Math.signum(d) * Math.floor(Math.abs(d));
    double int32bit = posInt % 4294967296.0;
    if (int32bit < 0)
      int32bit += 4294967296.0;
    if (int32bit >= 2147483648.0)
      int32bit -= 4294967296.0;
    return (int)int32bit;
  }

  public static V6Value bitAnd(V6Value a, V6Value b) {
    if (a.tag == TAG_BIGINT || b.tag == TAG_BIGINT) {
      if (a.tag != TAG_BIGINT || b.tag != TAG_BIGINT)
        throw mixedBigIntError();
      return bigint(a.asBigInt().and(b.asBigInt()));
    }
    return new V6Value(TAG_NUM, toInt32(a.toNumber()) & toInt32(b.toNumber()),
                       null);
  }

  public static V6Value bitOr(V6Value a, V6Value b) {
    if (a.tag == TAG_BIGINT || b.tag == TAG_BIGINT) {
      if (a.tag != TAG_BIGINT || b.tag != TAG_BIGINT)
        throw mixedBigIntError();
      return bigint(a.asBigInt().or(b.asBigInt()));
    }
    return new V6Value(TAG_NUM, toInt32(a.toNumber()) | toInt32(b.toNumber()),
                       null);
  }

  public static V6Value bitXor(V6Value a, V6Value b) {
    if (a.tag == TAG_BIGINT || b.tag == TAG_BIGINT) {
      if (a.tag != TAG_BIGINT || b.tag != TAG_BIGINT)
        throw mixedBigIntError();
      return bigint(a.asBigInt().xor(b.asBigInt()));
    }
    return new V6Value(TAG_NUM, toInt32(a.toNumber()) ^ toInt32(b.toNumber()),
                       null);
  }

  public static V6Value bitNot(V6Value a) {
    if (a.tag == TAG_BIGINT)
      return bigint(a.asBigInt().not());
    return new V6Value(TAG_NUM, ~toInt32(a.toNumber()), null);
  }

  public static V6Value shl(V6Value a, V6Value b) {
    if (a.tag == TAG_BIGINT || b.tag == TAG_BIGINT) {
      if (a.tag != TAG_BIGINT || b.tag != TAG_BIGINT)
        throw mixedBigIntError();
      return bigint(a.asBigInt().shiftLeft(b.asBigInt().intValueExact()));
    }
    int shift = toInt32(b.toNumber()) & 31;
    return new V6Value(TAG_NUM, toInt32(a.toNumber()) << shift, null);
  }

  public static V6Value shr(V6Value a, V6Value b) {
    if (a.tag == TAG_BIGINT || b.tag == TAG_BIGINT) {
      if (a.tag != TAG_BIGINT || b.tag != TAG_BIGINT)
        throw mixedBigIntError();
      return bigint(a.asBigInt().shiftRight(b.asBigInt().intValueExact()));
    }
    int shift = toInt32(b.toNumber()) & 31;
    return new V6Value(TAG_NUM, toInt32(a.toNumber()) >> shift, null);
  }

  public static V6Value ushr(V6Value a, V6Value b) {
    if (a.tag == TAG_BIGINT || b.tag == TAG_BIGINT) {
      if (a.tag != TAG_BIGINT || b.tag != TAG_BIGINT)
        throw mixedBigIntError();
      return bigint(a.asBigInt().shiftRight(b.asBigInt().intValueExact()));
    }
    int shift = toInt32(b.toNumber()) & 31;
    long result = (toInt32(a.toNumber()) >>> shift) & 0xFFFFFFFFL;
    return new V6Value(TAG_NUM, result, null);
  }

  public static boolean instanceOf(V6Value obj, V6Value ctor) {
    if (obj.tag != TAG_OBJ || ctor.tag != TAG_OBJ)
      return false;
    if (ctor.ref == V6Builtins.ARRAY.ref())
      return obj.ref instanceof V6Array;
    if (ctor.ref == V6Builtins.OBJECT.ref())
      return true;
    if (!(ctor.ref instanceof V6Class))
      return false;
    Object protoRef = ((V6Class)ctor.ref).get("prototype").ref();
    if (!(protoRef instanceof V6Object))
      return false;
    V6Object targetProto = (V6Object)protoRef;
    for (V6Object o = ((V6Object)obj.ref).getProto(); o != null; o = o.getProto()) {
      if (o == targetProto)
        return true;
    }
    return false;
  }

  public static boolean hasProp(V6Value key, V6Value obj) {
    return obj.tag == TAG_OBJ && ((V6Object)obj.ref).has(key.toString());
  }

  public static V6Value pow(V6Value a, V6Value b) {
    if (a.tag == TAG_BIGINT || b.tag == TAG_BIGINT) {
      if (a.tag != TAG_BIGINT || b.tag != TAG_BIGINT)
        throw mixedBigIntError();
      return bigint(a.asBigInt().pow(b.asBigInt().intValueExact()));
    }
    return new V6Value(TAG_NUM, Math.pow(a.toNumber(), b.toNumber()), null);
  }
}
