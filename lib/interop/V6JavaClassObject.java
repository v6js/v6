import java.lang.reflect.Field;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

public final class V6JavaClassObject
    extends V6Object implements V6NativeConstructor {
  final Class<?> clazz;
  private final Map<String, Field> fieldCache = new HashMap<>();
  private final Map<String, V6Value> props = new HashMap<>();

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
    Field cachedField = fieldCache.get(key);
    if (cachedField != null)
      return V6JavaMarshal.readField(cachedField, null);
    if (hasOwnNamed(key))
      return super.get(key);
    if (props.containsKey(key))
      return props.get(key);
    V6Value resolved = V6JavaMarshal.resolveStaticMethodOrNested(clazz, key);
    if (resolved != null) {
      props.put(key, resolved);
      return resolved;
    }
    Field f = V6JavaMarshal.staticFieldFor(clazz, key);
    if (f != null) {
      fieldCache.put(key, f);
      return V6JavaMarshal.readField(f, null);
    }
    return new V6Value(V6Value.TAG_UNDEF, 0, null);
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
