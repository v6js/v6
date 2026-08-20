public final class V6WasmModuleObject extends V6Object {
  public final String className;
  public final byte[] classBytes;
  public final String exportManifest;
  public final String importManifest;

  public V6WasmModuleObject(String className, byte[] classBytes,
                            String exportManifest, String importManifest) {
    this.className = className;
    this.classBytes = classBytes;
    this.exportManifest = exportManifest;
    this.importManifest = importManifest;
  }
}
