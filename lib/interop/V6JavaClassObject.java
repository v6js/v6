import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

public final class V6JavaClassObject
    extends V6Object implements V6NativeConstructor {
  final Class<?> clazz;

  private static final Map<Class<?>, V6JavaClassObject> CACHE =
      new ConcurrentHashMap<>();

  private V6JavaClassObject(Class<?> clazz) {
    this.clazz = clazz;
  }

  static V6Value wrap(Class<?> clazz) {
    V6JavaClassObject obj =
        CACHE.computeIfAbsent(clazz, V6JavaClassObject::new);
    return new V6Value(V6Value.TAG_OBJ, 0, obj);
  }

  @Override
  public V6Value get(String key) {
    if (props.containsKey(key))
      return props.get(key);
    V6Value resolved = V6JavaMarshal.resolveStaticMember(clazz, key);
    props.put(key, resolved);
    return resolved;
  }

  @Override
  public void set(String key, V6Value value) {
    if (!V6JavaMarshal.trySetStaticField(clazz, key, value))
      super.set(key, value);
  }

  @Override
  public V6Value construct(V6Value[] args) {
    return V6JavaMarshal.construct(clazz, args);
  }

  @Override
  public String toString() {
    return "[JavaClass " + clazz.getName() + "]";
  }
}
