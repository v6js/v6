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

  public static V6Value add(V6Value a, V6Value b) {
    if (a.tag == TAG_STR || b.tag == TAG_STR)
      return new V6Value(TAG_STR, 0, a.toString() + b.toString());
    return new V6Value(TAG_NUM, a.num + b.num, null);
  }
}
