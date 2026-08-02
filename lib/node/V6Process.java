public final class V6Process {
  private V6Process() {}

  private static V6Value str(String s) {
    return new V6Value(V6Value.TAG_STR, 0, s);
  }

  private static V6Value fn(V6Callable c) {
    return new V6Value(V6Value.TAG_FUNC, 0, c);
  }

  private static V6Value objValue(V6Object o) {
    return new V6Value(V6Value.TAG_OBJ, 0, o);
  }

  private static final V6Value UNDEF = new V6Value(V6Value.TAG_UNDEF, 0, null);

  private static final boolean IS_WINDOWS =
      System.getProperty("os.name", "").toLowerCase().contains("win");
  private static final boolean IS_MAC =
      System.getProperty("os.name", "").toLowerCase().contains("mac");

  private static String platformName() {
    if (IS_WINDOWS)
      return "win32";
    if (IS_MAC)
      return "darwin";
    return "linux";
  }

  private static V6Object streamObject(java.io.PrintStream stream) {
    V6Object o = new V6Object();
    o.set("write", fn((thisArg, args) -> {
            stream.print(V6Value.argAt(args, 0).toString());
            return new V6Value(V6Value.TAG_BOOL, 1, null);
          }));
    return o;
  }

  public static V6Object build() {
    V6Object o = new V6Object();

    V6Array argv = new V6Array();
    argv.push(str("v6"));
    o.set("argv", objValue(argv));

    V6Object env = new V6Object();
    for (java.util.Map.Entry<String, String> e : System.getenv().entrySet())
      env.set(e.getKey(), str(e.getValue()));
    o.set("env", objValue(env));

    o.set("platform", str(platformName()));
    o.set("version", str("v6.0.0"));
    V6Object versions = new V6Object();
    versions.set("v6", str("0.1.0"));
    versions.set("node", str("20.0.0"));
    o.set("versions", objValue(versions));

    o.set("cwd", fn((thisArg, args) -> str(System.getProperty("user.dir", "."))));

    o.set("exit", fn((thisArg, args) -> {
            int code = args.length > 0 ? (int)args[0].toNumber() : 0;
            System.exit(code);
            return UNDEF;
          }));

    o.set("nextTick", fn((thisArg, args) -> {
            V6Callable cb = V6Value.argAt(args, 0).asCallable();
            V6Value[] rest = new V6Value[Math.max(0, args.length - 1)];
            System.arraycopy(args, 1, rest, 0, rest.length);
            V6MicrotaskQueue.enqueue(() -> cb.call(UNDEF, rest));
            return UNDEF;
          }));

    o.set("stdout", objValue(streamObject(System.out)));
    o.set("stderr", objValue(streamObject(System.err)));

    o.set("hrtime", fn((thisArg, args) -> {
            long nanos = System.nanoTime();
            V6Array result = new V6Array();
            result.push(new V6Value(V6Value.TAG_NUM, nanos / 1_000_000_000L, null));
            result.push(new V6Value(V6Value.TAG_NUM, nanos % 1_000_000_000L, null));
            return objValue(result);
          }));

    return o;
  }
}
