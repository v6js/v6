public final class V6JavaPackageObject extends V6Object {
  private final String prefix;

  public V6JavaPackageObject(String prefix) {
    this.prefix = prefix;
  }

  @Override
  public V6Value get(String key) {
    if (props.containsKey(key))
      return props.get(key);
    String full = prefix + "." + key;
    V6Value resolved;
    if (!key.isEmpty() && Character.isUpperCase(key.charAt(0))) {
      try {
        resolved = V6JavaInterop.classFor(full);
      } catch (V6Throw e) {
        resolved =
            new V6Value(V6Value.TAG_OBJ, 0, new V6JavaPackageObject(full));
      }
    } else {
      resolved = new V6Value(V6Value.TAG_OBJ, 0, new V6JavaPackageObject(full));
    }
    props.put(key, resolved);
    return resolved;
  }

  @Override
  public String toString() {
    return "[JavaPackage " + prefix + "]";
  }
}
