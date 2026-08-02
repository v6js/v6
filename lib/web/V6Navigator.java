public final class V6Navigator {
  private V6Navigator() {
  }

  private static V6Value str(String s) {
    return new V6Value(V6Value.TAG_STR, 0, s);
  }

  private static V6Value num(double n) {
    return new V6Value(V6Value.TAG_NUM, n, null);
  }

  public static V6Object build() {
    V6Object o = new V6Object();
    o.set("hardwareConcurrency",
          num(Runtime.getRuntime().availableProcessors()));
    o.set("userAgent", str("v6"));
    o.set("language", str("en-US"));
    o.set("languages", languagesArray());
    o.set("platform", str(System.getProperty("os.name", "unknown")));
    return o;
  }

  private static V6Value languagesArray() {
    V6Array arr = new V6Array();
    arr.push(str("en-US"));
    return new V6Value(V6Value.TAG_OBJ, 0, arr);
  }
}
