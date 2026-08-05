public final class V6GlobalDispatchObject extends V6Object {
  private static final InheritableThreadLocal<V6Object> CURRENT =
      new InheritableThreadLocal<>();

  public static void resetForThread() {
    CURRENT.set(new V6Object());
  }

  public static void clearForThread() {
    CURRENT.remove();
  }

  private static V6Object current() {
    V6Object v = CURRENT.get();
    if (v == null) {
      v = new V6Object();
      CURRENT.set(v);
    }
    return v;
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
  public V6Array enumKeys() {
    return current().enumKeys();
  }

  @Override
  public V6Object getOwnPropertyDescriptor(String key) {
    return current().getOwnPropertyDescriptor(key);
  }

  @Override
  public void defineGetter(String key, V6Callable getter) {
    current().defineGetter(key, getter);
  }

  @Override
  public void defineSetter(String key, V6Callable setter) {
    current().defineSetter(key, setter);
  }

  @Override
  public void freeze() {
    current().freeze();
  }

  @Override
  public boolean isFrozenFlag() {
    return current().isFrozenFlag();
  }

  @Override
  public void seal() {
    current().seal();
  }

  @Override
  public boolean isSealedFlag() {
    return current().isSealedFlag();
  }

  @Override
  public String toString() {
    return current().toString();
  }
}
