public final class V6JavaInterop {
  private V6JavaInterop() {
  }

  static volatile Thread mainThread;

  private static void ensureMainThread() {
    if (mainThread == null)
      mainThread = Thread.currentThread();
  }

  static boolean onMainThread() {
    return Thread.currentThread() == mainThread;
  }

  public static V6Value classFor(String fqcn) {
    ensureMainThread();
    try {
      Class<?> cls =
          Class.forName(fqcn, true, V6JavaInterop.class.getClassLoader());
      return V6JavaClassObject.wrap(cls);
    } catch (ClassNotFoundException e) {
      throw new V6Throw(new V6Value(V6Value.TAG_STR, 0,
                                    "Error: java class not found: " + fqcn));
    }
  }

  public static V6Value packageFor(String pkgName) {
    ensureMainThread();
    return new V6Value(V6Value.TAG_OBJ, 0, new V6JavaPackageObject(pkgName));
  }
}
