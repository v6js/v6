public final class V6ProcessDispatchObject extends V6Object {
  private static final InheritableThreadLocal<V6EventEmitterObject> CURRENT =
      new InheritableThreadLocal<>();

  public static void bindForThread(V6EventEmitterObject real) {
    CURRENT.set(real);
  }

  public static void clearForThread() {
    CURRENT.remove();
  }

  public static V6EventEmitterObject currentOrNull() {
    return CURRENT.get();
  }

  private static V6EventEmitterObject current() {
    V6EventEmitterObject real = CURRENT.get();
    if (real == null) {
      real = V6Process.build();
      CURRENT.set(real);
    }
    return real;
  }

  @Override
  public V6Value get(String key) {
    return current().get(key);
  }

  @Override
  public void set(String key, V6Value value) {
    current().set(key, value);
  }

  @Override
  public boolean has(String key) {
    return current().has(key);
  }

  @Override
  public boolean hasOwn(String key) {
    return current().hasOwn(key);
  }

  @Override
  public boolean hasOwnNamed(String key) {
    return current().hasOwnNamed(key);
  }

  @Override
  public boolean delete(String key) {
    return current().delete(key);
  }

  @Override
  public java.util.Set<String> keySet() {
    return current().keySet();
  }

  @Override
  public V6Object getOwnPropertyDescriptor(String key) {
    return current().getOwnPropertyDescriptor(key);
  }

  @Override
  public String toString() {
    return current().toString();
  }

  @Override
  public V6Object resolveEmitterTarget() {
    return current();
  }
}
