public final class V6Process {
  private V6Process() {
  }

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
  public static volatile String scriptPath = "";
  private static boolean stdinStarted = false;

  public static void setArgv(String[] args) {
    rawArgv = args;
  }

  public static void setScriptPath(String path) {
    scriptPath = path;
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
        java.io.BufferedReader r = new java.io.BufferedReader(
            new java.io.InputStreamReader(System.in));
        String line;
        while ((line = r.readLine()) != null) {
          final String chunk = line + "\n";
          V6EventLoop.postExternal(() -> {
            V6Value dataVal =
                encodingHolder[0] != null
                    ? str(chunk)
                    : objValue(new V6Buffer(chunk.getBytes(
                          java.nio.charset.StandardCharsets.UTF_8)));
            s.get("emit").asCallable().call(
                objValue(s), new V6Value[] {str("data"), dataVal});
          });
        }
      } catch (java.io.IOException ignored) {
      } finally {
        V6EventLoop.postExternal(
            ()
                -> s.get("emit").asCallable().call(objValue(s),
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
      if (!scriptPath.isEmpty())
        argv.push(str(scriptPath));
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

    o.set("cwd",
          fn((thisArg, args) -> str(System.getProperty("user.dir", "."))));

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

    boolean isForkChild = System.getenv("V6_FORK_MODE") != null ||
                          System.getenv("V6_CLUSTER_WORKER") != null;
    o.set("connected",
          new V6Value(V6Value.TAG_BOOL, isForkChild ? 1 : 0, null));
    if (isForkChild) {
      o.set("send", fn((thisArg, args) -> {
              V6IpcUtil.sendMessage(System.out, V6Value.argAt(args, 0));
              return new V6Value(V6Value.TAG_BOOL, 1, null);
            }));
      o.set("disconnect", fn((thisArg, args) -> {
              o.set("connected", new V6Value(V6Value.TAG_BOOL, 0, null));
              return UNDEF;
            }));
      V6IpcUtil.pumpMessages(
          System.in, null,
          msg
          -> o.get("emit").asCallable().call(
              objValue(o), new V6Value[] {str("message"), msg}));
    }

    o.set("hrtime", fn((thisArg, args) -> {
            long nanos = System.nanoTime();
            V6Array result = new V6Array();
            result.push(
                new V6Value(V6Value.TAG_NUM, nanos / 1_000_000_000L, null));
            result.push(
                new V6Value(V6Value.TAG_NUM, nanos % 1_000_000_000L, null));
            return objValue(result);
          }));

    V6NativeFunctionObject memoryUsageObj = new V6NativeFunctionObject(
        (thisArg, args) -> objValue(memoryUsageObject()));
    memoryUsageObj.set(
        "rss",
        fn((thisArg, args) -> new V6Value(V6Value.TAG_NUM, rss(), null)));
    o.set("memoryUsage", new V6Value(V6Value.TAG_FUNC, 0, memoryUsageObj));

    o.set(
        "cpuUsage", fn((thisArg, args) -> {
          long[] usage = processCpuMicros();
          double prevUser = 0, prevSystem = 0;
          V6Value prev = V6Value.argAt(args, 0);
          if (prev.tag() == V6Value.TAG_OBJ && prev.ref() instanceof V6Object) {
            prevUser = ((V6Object)prev.ref()).get("user").toNumber();
            prevSystem = ((V6Object)prev.ref()).get("system").toNumber();
          }
          V6Object result = new V6Object();
          result.set("user",
                     new V6Value(V6Value.TAG_NUM, usage[0] - prevUser, null));
          result.set("system",
                     new V6Value(V6Value.TAG_NUM, usage[1] - prevSystem, null));
          return objValue(result);
        }));

    o.set("kill", fn((thisArg, args) -> {
            long pid = (long)V6Value.argAt(args, 0).toNumber();
            java.util.Optional<ProcessHandle> ph = ProcessHandle.of(pid);
            if (ph.isPresent())
              ph.get().destroy();
            return new V6Value(V6Value.TAG_BOOL, 1, null);
          }));

    wireSignalHandlers(o);

    return o;
  }

  private static long rss() {
    try {
      com.sun.management.OperatingSystemMXBean osBean =
          (com.sun.management.OperatingSystemMXBean)
              java.lang.management.ManagementFactory.getOperatingSystemMXBean();
      long total = osBean.getTotalMemorySize() - osBean.getFreeMemorySize();
      return total > 0 ? total : Runtime.getRuntime().totalMemory();
    } catch (Throwable t) {
      return Runtime.getRuntime().totalMemory();
    }
  }

  private static V6Object memoryUsageObject() {
    Runtime rt = Runtime.getRuntime();
    long heapTotal = rt.totalMemory();
    long heapUsed = heapTotal - rt.freeMemory();
    V6Object result = new V6Object();
    result.set("rss", new V6Value(V6Value.TAG_NUM, rss(), null));
    result.set("heapTotal", new V6Value(V6Value.TAG_NUM, heapTotal, null));
    result.set("heapUsed", new V6Value(V6Value.TAG_NUM, heapUsed, null));
    result.set("external", new V6Value(V6Value.TAG_NUM, 0, null));
    result.set("arrayBuffers", new V6Value(V6Value.TAG_NUM, 0, null));
    return result;
  }

  private static long[] processCpuMicros() {
    try {
      com.sun.management.OperatingSystemMXBean osBean =
          (com.sun.management.OperatingSystemMXBean)
              java.lang.management.ManagementFactory.getOperatingSystemMXBean();
      long userMicros = osBean.getProcessCpuTime() / 1000;
      return new long[] {userMicros, 0};
    } catch (Throwable t) {
      return new long[] {0, 0};
    }
  }

  private static void wireSignalHandlers(V6EventEmitterObject o) {
    String[] names = {"INT", "TERM", "HUP", "BREAK"};
    for (String n : names) {
      try {
        sun.misc.Signal.handle(new sun.misc.Signal(n), sig -> {
          V6EventLoop.postExternal(
              ()
                  -> o.get("emit").asCallable().call(
                      objValue(o), new V6Value[] {str("SIG" + n)}));
        });
      } catch (IllegalArgumentException ignored) {
      }
    }
  }
}
