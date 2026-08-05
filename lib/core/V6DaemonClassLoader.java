public final class V6DaemonClassLoader extends ClassLoader {
  public V6DaemonClassLoader(ClassLoader parent) {
    super(parent);
  }

  public Class<?> defineFromBytes(String name, byte[] bytes) {
    return defineClass(name, bytes, 0, bytes.length);
  }
}
