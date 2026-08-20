public final class V6WasiCliRunner {
  private static final V6Value UNDEF = new V6Value(V6Value.TAG_UNDEF, 0, null);

  private static V6Value objValue(Object o) {
    return new V6Value(V6Value.TAG_OBJ, 0, o);
  }

  private static V6WasiObject buildFullWasi(String[] args, boolean allowArgs,
                                            boolean allowEnv,
                                            boolean allowRandom,
                                            boolean allowClock) {
    V6WasiObject wasi = new V6WasiObject();
    if (allowArgs) {
      for (String a : args)
        wasi.args.add(a);
    }
    if (allowEnv) {
      for (java.util.Map.Entry<String, String> e : System.getenv().entrySet())
        wasi.envPairs.add(e.getKey() + "=" + e.getValue());
    }
    V6Object wasiImport = V6WasiConstructor.buildWasiImport(wasi);
    if (!allowRandom)
      wasiImport.delete("random_get");
    if (!allowClock)
      wasiImport.delete("clock_time_get");
    wasi.set("wasiImport", objValue(wasiImport));
    return wasi;
  }

  public static void run(byte[] wasmBytes, String[] args, int flags) {
    try {
      runInner(wasmBytes, args, flags);
    } catch (V6ProcessExit e) {
      throw e;
    } catch (RuntimeException e) {
      System.err.println("error: " + e.getMessage());
      System.exit(1);
    }
  }

  private static void runInner(byte[] wasmBytes, String[] args, int flags) {
    boolean allowArgs = (flags & 1) != 0;
    boolean allowEnv = (flags & 2) != 0;
    boolean allowRandom = (flags & 4) != 0;
    boolean allowClock = (flags & 8) != 0;

    V6WasmModuleObject module = V6WasmGlobal.compileModule(wasmBytes);
    boolean needsWasi =
        module.importManifest != null && !module.importManifest.isEmpty();

    V6WasiObject wasi = null;
    V6Value importObj = null;
    if (needsWasi) {
      wasi = buildFullWasi(args, allowArgs, allowEnv, allowRandom, allowClock);
      V6Object importsRoot = new V6Object();
      importsRoot.set("wasi_snapshot_preview1", wasi.get("wasiImport"));
      importObj = objValue(importsRoot);
    }

    V6WasmInstanceObject instance =
        V6WasmGlobal.instantiateModule(module, importObj);
    V6Value exportsVal = instance.get("exports");

    String entryName = null;
    if (exportsVal.getProp("_start").tag() == V6Value.TAG_FUNC)
      entryName = "_start";
    else if (exportsVal.getProp("_initialize").tag() == V6Value.TAG_FUNC)
      entryName = "_initialize";

    if (entryName != null) {
      if (wasi != null)
        wasi.hookMemory(objValue(instance));
      V6Value fn = exportsVal.getProp(entryName);
      fn.call(UNDEF, new V6Value[0]);
      return;
    }

    String[] lines = module.exportManifest.split("\n");
    String onlyExportName = null;
    int exportCount = 0;
    for (String line : lines) {
      if (line.isEmpty())
        continue;
      exportCount++;
      onlyExportName = line.split("\t")[0];
    }
    if (exportCount == 1) {
      V6Value fn = exportsVal.getProp(onlyExportName);
      V6Value result = fn.call(UNDEF, new V6Value[0]);
      if (result.tag() != V6Value.TAG_UNDEF)
        System.out.println(result.toString());
      return;
    }

    throw new RuntimeException(
        "error: no entry point found (no _start/_initialize export and "
        + "not exactly one exported function)");
  }
}
