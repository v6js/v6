public final class V6WasmCompiler {
  public static native byte[] compile(byte[] wasmBytes, String className);
  public static native String describeExports(byte[] wasmBytes);
  public static native String describeImports(byte[] wasmBytes);
}
