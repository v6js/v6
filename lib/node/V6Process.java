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

  private static V6EventEmitterObject processObj;
  public static volatile String[] rawArgv = new String[0];
  private static boolean stdinStarted = false;

  public static void setArgv(String[] args) {
    rawArgv = args;
  }

  public static void dispatchExit(int code) {
    if (processObj == null)
      return;
    processObj.get("emit").asCallable().call(
        objValue(processObj),
        new V6Value[] {str("exit"), new V6Value(V6Value.TAG_NUM, code, null)});
  }

  public static boolean dispatchUncaught(V6Value err) {
    if (processObj == null)
      return false;
    int count = (int)processObj.get("listenerCount")
                    .asCallable()
                    .call(objValue(processObj),
                          new V6Value[] {str("uncaughtException")})
                    .toNumber();
    if (count == 0)
      return false;
    processObj.get("emit").asCallable().call(
        objValue(processObj), new V6Value[] {str("uncaughtException"), err});
    return true;
  }

  private static synchronized void ensureStdinStarted(V6EventEmitterObject s,
                                                       String[] encodingHolder) {
    if (stdinStarted)
      return;
    stdinStarted = true;
    V6EventLoop.ref();
    Thread th = new Thread(() -> {
      try {
        java.io.BufferedReader r =
            new java.io.BufferedReader(new java.io.InputStreamReader(System.in));
        String line;
        while ((line = r.readLine()) != null) {
          final String chunk = line + "\n";
          V6EventLoop.postExternal(() -> {
            V6Value dataVal =
                encodingHolder[0] != null
                    ? str(chunk)
                    : objValue(new V6Buffer(
                          chunk.getBytes(java.nio.charset.StandardCharsets.UTF_8)));
            s.get("emit").asCallable().call(
                objValue(s), new V6Value[] {str("data"), dataVal});
          });
        }
      } catch (java.io.IOException ignored) {
      } finally {
        V6EventLoop.postExternal(
            () -> s.get("emit").asCallable().call(objValue(s),
                                                  new V6Value[] {str("end")}));
        V6EventLoop.unref();
      }
    });
    th.setDaemon(true);
    th.start();
  }

  private static V6Object buildStdin() {
    V6EventEmitterObject s = new V6EventEmitterObject();
    s.setProto(V6EventEmitterConstructor.PROTOTYPE);
    s.set("readable", new V6Value(V6Value.TAG_BOOL, 1, null));
    String[] encodingHolder = new String[1];
    s.set("setEncoding", fn((t, a) -> {
            encodingHolder[0] = a.length > 0 ? a[0].toString() : null;
            return t;
          }));
    s.set("resume", fn((t, a) -> {
            ensureStdinStarted(s, encodingHolder);
            return t;
          }));
    s.set("pause", fn((t, a) -> t));
    s.set("on", fn((t, a) -> {
            String event = V6Value.argAt(a, 0).toString();
            if (event.equals("data") || event.equals("readable"))
              ensureStdinStarted(s, encodingHolder);
            return V6EventEmitterConstructor.PROTOTYPE.get("on")
                .asCallable()
                .call(t, a);
          }));
    return s;
  }

  public static V6Object build() {
    V6EventEmitterObject o = new V6EventEmitterObject();
    o.setProto(V6EventEmitterConstructor.PROTOTYPE);
    processObj = o;

    o.defineGetter("argv", (t, a) -> {
      V6Array argv = new V6Array();
      argv.push(str("v6"));
      for (String s : rawArgv)
        argv.push(str(s));
      return objValue(argv);
    });

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
            dispatchExit(code);
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
    o.set("stdin", objValue(buildStdin()));

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
