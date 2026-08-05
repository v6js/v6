public final class V6JavaInstanceObject extends V6Object {
  public final Object instance;
  private final java.util.Map<String, java.lang.reflect.Field> fieldCache =
      new java.util.HashMap<>();

  public V6JavaInstanceObject(Object instance) {
    this.instance = instance;
  }

  @Override
  public V6Value get(String key) {
    java.lang.reflect.Field cachedField = fieldCache.get(key);
    if (cachedField != null)
      return V6JavaMarshal.readField(cachedField, instance);
    if (hasOwnNamed(key))
      return super.get(key);
    if (props.containsKey(key))
      return props.get(key);
    V6Value method = V6JavaMarshal.resolveInstanceMethod(instance, key);
    if (method != null) {
      props.put(key, method);
      return method;
    }
    java.lang.reflect.Field f =
        V6JavaMarshal.instanceFieldFor(instance.getClass(), key);
    if (f != null) {
      fieldCache.put(key, f);
      return V6JavaMarshal.readField(f, instance);
    }
    return new V6Value(V6Value.TAG_UNDEF, 0, null);
  }

  @Override
  public void set(String key, V6Value value) {
    if (!V6JavaMarshal.trySetInstanceField(instance, key, value))
      super.set(key, value);
  }

  @Override
  public String toString() {
    return String.valueOf(instance);
  }
}
