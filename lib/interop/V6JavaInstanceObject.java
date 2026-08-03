public final class V6JavaInstanceObject extends V6Object {
  public final Object instance;

  public V6JavaInstanceObject(Object instance) {
    this.instance = instance;
  }

  @Override
  public V6Value get(String key) {
    if (props.containsKey(key))
      return props.get(key);
    V6Value resolved = V6JavaMarshal.resolveInstanceMember(instance, key);
    props.put(key, resolved);
    return resolved;
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
