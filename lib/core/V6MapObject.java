import java.util.LinkedHashMap;
import java.util.Map;

public final class V6MapObject extends V6Object {
  private static final Object NULL_KEY = new Object();
  private static final Object UNDEF_KEY = new Object();

  final LinkedHashMap<Object, V6Value> entries = new LinkedHashMap<>();

  static Object keyFor(V6Value v) {
    switch (v.tag()) {
    case V6Value.TAG_STR:
      return v.toString();
    case V6Value.TAG_NUM:
      return v.num();
    case V6Value.TAG_BOOL:
      return v.num() != 0;
    case V6Value.TAG_NULL:
      return NULL_KEY;
    case V6Value.TAG_UNDEF:
      return UNDEF_KEY;
    default:
      return v.ref();
    }
  }

  static V6Value keyToValue(Object rawKey) {
    if (rawKey == NULL_KEY)
      return new V6Value(V6Value.TAG_NULL, 0, null);
    if (rawKey == UNDEF_KEY)
      return new V6Value(V6Value.TAG_UNDEF, 0, null);
    if (rawKey instanceof String)
      return new V6Value(V6Value.TAG_STR, 0, rawKey);
    if (rawKey instanceof Double)
      return new V6Value(V6Value.TAG_NUM, (Double)rawKey, null);
    if (rawKey instanceof Boolean)
      return new V6Value(V6Value.TAG_BOOL, ((Boolean)rawKey) ? 1 : 0, null);
    if (rawKey instanceof V6Object)
      return new V6Value(V6Value.TAG_OBJ, 0, rawKey);
    return new V6Value(V6Value.TAG_FUNC, 0, rawKey);
  }

  public V6MapObject() {
    length = 0;
  }
}
