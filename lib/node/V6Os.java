public final class V6Os {
  private V6Os() {}

  private static V6Value str(String s) {
    return new V6Value(V6Value.TAG_STR, 0, s);
  }

  private static V6Value num(double n) {
    return new V6Value(V6Value.TAG_NUM, n, null);
  }

  private static V6Value fn(V6Callable c) {
    return new V6Value(V6Value.TAG_FUNC, 0, c);
  }

  private static V6Value objValue(V6Object o) {
    return new V6Value(V6Value.TAG_OBJ, 0, o);
  }

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

  private static String archName() {
    String arch = System.getProperty("os.arch", "").toLowerCase();
    if (arch.contains("aarch64") || arch.contains("arm64"))
      return "arm64";
    if (arch.contains("64"))
      return "x64";
    if (arch.contains("86"))
      return "ia32";
    return arch.isEmpty() ? "unknown" : arch;
  }

  private static String typeName() {
    if (IS_WINDOWS)
      return "Windows_NT";
    if (IS_MAC)
      return "Darwin";
    return "Linux";
  }

  private static com.sun.management.OperatingSystemMXBean osBean() {
    return (com.sun.management.OperatingSystemMXBean)java.lang.management
        .ManagementFactory.getOperatingSystemMXBean();
  }

  private static String cpuModel() {
    if (IS_WINDOWS) {
      String id = System.getenv("PROCESSOR_IDENTIFIER");
      return id != null ? id : "unknown";
    }
    try {
      for (String line :
           java.nio.file.Files.readAllLines(java.nio.file.Paths.get("/proc/cpuinfo"))) {
        if (line.startsWith("model name")) {
          int idx = line.indexOf(':');
          if (idx >= 0)
            return line.substring(idx + 1).trim();
        }
      }
    } catch (Exception ignored) {
    }
    return "unknown";
  }

  private static double cpuSpeedMHz() {
    if (!IS_WINDOWS) {
      try {
        for (String line : java.nio.file.Files.readAllLines(
                 java.nio.file.Paths.get("/proc/cpuinfo"))) {
          if (line.startsWith("cpu MHz")) {
            int idx = line.indexOf(':');
            if (idx >= 0)
              return Double.parseDouble(line.substring(idx + 1).trim());
          }
        }
      } catch (Exception ignored) {
      }
    }
    return 0;
  }

  public static V6Object build() {
    V6Object o = new V6Object();

    o.set("platform", fn((thisArg, args) -> str(platformName())));
    o.set("arch", fn((thisArg, args) -> str(archName())));
    o.set("type", fn((thisArg, args) -> str(typeName())));
    o.set("release", fn((thisArg, args) -> str(System.getProperty("os.version", ""))));
    o.set("homedir", fn((thisArg, args) -> str(System.getProperty("user.home", ""))));
    o.set("tmpdir",
          fn((thisArg, args) -> str(System.getProperty("java.io.tmpdir", ""))));
    o.set("hostname", fn((thisArg, args) -> {
            try {
              return str(java.net.InetAddress.getLocalHost().getHostName());
            } catch (java.net.UnknownHostException e) {
              return str("localhost");
            }
          }));
    o.set("cpus", fn((thisArg, args) -> {
            int n = Runtime.getRuntime().availableProcessors();
            String model = cpuModel();
            double speed = cpuSpeedMHz();
            V6Array result = new V6Array();
            for (int i = 0; i < n; i++) {
              V6Object cpu = new V6Object();
              cpu.set("model", str(model));
              cpu.set("speed", num(speed));
              V6Object times = new V6Object();
              times.set("user", num(0));
              times.set("nice", num(0));
              times.set("sys", num(0));
              times.set("idle", num(0));
              times.set("irq", num(0));
              cpu.set("times", objValue(times));
              result.push(objValue(cpu));
            }
            return objValue(result);
          }));
    o.set("totalmem", fn((thisArg, args) -> num(osBean().getTotalMemorySize())));
    o.set("freemem", fn((thisArg, args) -> num(osBean().getFreeMemorySize())));
    o.set("EOL", str(IS_WINDOWS ? "\r\n" : "\n"));
    o.set("endianness",
          fn((thisArg, args)
                 -> str(java.nio.ByteOrder.nativeOrder() == java.nio.ByteOrder.BIG_ENDIAN
                            ? "BE"
                            : "LE")));

    return o;
  }
}
