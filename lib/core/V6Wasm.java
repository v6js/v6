public final class V6Wasm {
  public static void trapUnreachable() {
    throw new RuntimeException("wasm: unreachable");
  }

  public static float f32ceil(float v) {
    return (float)Math.ceil((double)v);
  }

  public static float f32floor(float v) {
    return (float)Math.floor((double)v);
  }

  public static float f32trunc(float v) {
    return v < 0 ? (float)Math.ceil((double)v) : (float)Math.floor((double)v);
  }

  public static float f32nearest(float v) {
    return (float)Math.rint((double)v);
  }

  public static float f32sqrt(float v) {
    return (float)Math.sqrt((double)v);
  }

  public static double f64trunc(double v) {
    return v < 0 ? Math.ceil(v) : Math.floor(v);
  }

  public static int selectI32(int a, int b, int cond) {
    return cond != 0 ? a : b;
  }

  public static long selectI64(long a, long b, int cond) {
    return cond != 0 ? a : b;
  }

  public static float selectF32(float a, float b, int cond) {
    return cond != 0 ? a : b;
  }

  public static double selectF64(double a, double b, int cond) {
    return cond != 0 ? a : b;
  }
}
