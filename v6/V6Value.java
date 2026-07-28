public record V6Value(int tag, double num, Object ref) {
  public static final int TAG_NUM = 0;
  public static final int TAG_BOOL = 1;
  public static final int TAG_NULL = 2;
  public static final int TAG_UNDEF = 3;
  public static final int TAG_OBJ = 4;
  public static final int TAG_STR = 5;

  @Override
  public String toString() {
    return switch (tag) {
      case TAG_NUM -> numToString(num);
      case TAG_BOOL -> num != 0 ? "true" : "false";
      case TAG_NULL -> "null";
      case TAG_UNDEF -> "undefined";
      case TAG_STR -> (String) ref;
      default -> String.valueOf(ref);
    };
  }

  private static String numToString(double n) {
    if (Double.isNaN(n))
      return "NaN";
    if (Double.isInfinite(n))
      return n > 0 ? "Infinity" : "-Infinity";
    if (n == Math.rint(n))
      return Long.toString((long)n);
    return Double.toString(n);
  }

  public boolean truthy() {
    return switch (tag) {
      case TAG_NUM -> num != 0 && !Double.isNaN(num);
      case TAG_BOOL -> num != 0;
      case TAG_NULL, TAG_UNDEF -> false;
      case TAG_STR -> !((String)ref).isEmpty();
      default -> true;
    };
  }

  public double toNumber() {
    return switch (tag) {
      case TAG_NUM, TAG_BOOL -> num;
      case TAG_NULL -> 0;
      case TAG_UNDEF -> Double.NaN;
      case TAG_STR -> parseNumericString((String)ref);
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

  public static V6Value add(V6Value a, V6Value b) {
    if (a.tag == TAG_STR || b.tag == TAG_STR)
      return new V6Value(TAG_STR, 0, a.toString() + b.toString());
    return new V6Value(TAG_NUM, a.toNumber() + b.toNumber(), null);
  }

  public static boolean lt(V6Value a, V6Value b) {
    if (a.tag == TAG_STR && b.tag == TAG_STR)
      return ((String)a.ref).compareTo((String)b.ref) < 0;
    return a.toNumber() < b.toNumber();
  }

  public static boolean le(V6Value a, V6Value b) {
    if (a.tag == TAG_STR && b.tag == TAG_STR)
      return ((String)a.ref).compareTo((String)b.ref) <= 0;
    return a.toNumber() <= b.toNumber();
  }

  public static boolean gt(V6Value a, V6Value b) {
    if (a.tag == TAG_STR && b.tag == TAG_STR)
      return ((String)a.ref).compareTo((String)b.ref) > 0;
    return a.toNumber() > b.toNumber();
  }

  public static boolean ge(V6Value a, V6Value b) {
    if (a.tag == TAG_STR && b.tag == TAG_STR)
      return ((String)a.ref).compareTo((String)b.ref) >= 0;
    return a.toNumber() >= b.toNumber();
  }

  public static boolean strictEquals(V6Value a, V6Value b) {
    if (a.tag != b.tag)
      return false;
    return switch (a.tag) {
      case TAG_NUM, TAG_BOOL -> a.num == b.num;
      case TAG_STR -> a.ref.equals(b.ref);
      case TAG_NULL, TAG_UNDEF -> true;
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
    return a.toNumber() == b.toNumber();
  }
}
