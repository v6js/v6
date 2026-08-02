import java.util.IdentityHashMap;
import java.util.Map;

public final class V6StructuredClone {
  private V6StructuredClone() {
  }

  private static V6Value str(String s) {
    return new V6Value(V6Value.TAG_STR, 0, s);
  }

  private static V6Value objValue(V6Object o) {
    return new V6Value(V6Value.TAG_OBJ, 0, o);
  }

  public static V6Value clone(V6Value v) {
    return cloneInternal(v, new IdentityHashMap<>());
  }

  private static V6Value cloneInternal(V6Value v,
                                       IdentityHashMap<Object, V6Value> seen) {
    switch (v.tag()) {
    case V6Value.TAG_NUM:
    case V6Value.TAG_BOOL:
    case V6Value.TAG_STR:
    case V6Value.TAG_NULL:
    case V6Value.TAG_UNDEF:
    case V6Value.TAG_BIGINT:
      return v;
    case V6Value.TAG_FUNC:
      throw new V6Throw(str("DataCloneError: could not be cloned (function)"));
    case V6Value.TAG_OBJ:
      return cloneObject(v, seen);
    default:
      return v;
    }
  }

  private static V6Value cloneObject(V6Value v,
                                     IdentityHashMap<Object, V6Value> seen) {
    Object ref = v.ref();
    if (seen.containsKey(ref))
      return seen.get(ref);

    if (ref instanceof V6Symbol)
      throw new V6Throw(str("DataCloneError: symbols could not be cloned"));

    if (ref instanceof V6Buffer) {
      byte[] src = ((V6Buffer)ref).toBytes();
      V6Value result = objValue(new V6Buffer(src.clone()));
      seen.put(ref, result);
      return result;
    }

    if (ref instanceof V6DateObject) {
      V6DateObject src = (V6DateObject)ref;
      V6DateObject copy = new V6DateObject(src.epochMillis);
      copy.setProto(V6DateConstructor.PROTOTYPE);
      V6Value result = objValue(copy);
      seen.put(ref, result);
      return result;
    }

    if (ref instanceof V6Regex) {
      V6Regex src = (V6Regex)ref;
      StringBuilder flags = new StringBuilder();
      if (src.global)
        flags.append('g');
      if ((src.pattern.flags() & java.util.regex.Pattern.CASE_INSENSITIVE) != 0)
        flags.append('i');
      if ((src.pattern.flags() & java.util.regex.Pattern.MULTILINE) != 0)
        flags.append('m');
      if ((src.pattern.flags() & java.util.regex.Pattern.DOTALL) != 0)
        flags.append('s');
      if (src.sticky)
        flags.append('y');
      V6Regex copy = new V6Regex(src.pattern.pattern(), flags.toString());
      copy.setProto(V6Builtins.REGEXP_PROTOTYPE);
      V6Value result = objValue(copy);
      seen.put(ref, result);
      return result;
    }

    if (ref instanceof V6MapObject) {
      V6MapObject src = (V6MapObject)ref;
      V6MapObject copy = new V6MapObject();
      copy.setProto(src.getProto());
      V6Value result = objValue(copy);
      seen.put(ref, result);
      for (Map.Entry<Object, V6Value> e : src.entries.entrySet()) {
        V6Value key = cloneInternal(V6MapObject.keyToValue(e.getKey()), seen);
        V6Value val = cloneInternal(e.getValue(), seen);
        copy.entries.put(V6MapObject.keyFor(key), val);
      }
      return result;
    }

    if (ref instanceof V6SetObject) {
      V6SetObject src = (V6SetObject)ref;
      V6SetObject copy = new V6SetObject();
      copy.setProto(src.getProto());
      V6Value result = objValue(copy);
      seen.put(ref, result);
      for (Map.Entry<Object, V6Value> e : src.entries.entrySet()) {
        V6Value member = cloneInternal(e.getValue(), seen);
        copy.entries.put(V6MapObject.keyFor(member), member);
      }
      return result;
    }

    if (ref instanceof V6Array) {
      V6Array src = (V6Array)ref;
      V6Array copy = new V6Array();
      V6Value result = objValue(copy);
      seen.put(ref, result);
      for (int i = 0; i < src.elemCount; i++)
        copy.push(cloneInternal(src.elements[i], seen));
      return result;
    }

    if (ref instanceof V6Object && !(ref instanceof V6Closure) &&
        !(ref instanceof V6Class)) {
      V6Object src = (V6Object)ref;
      V6Object copy = new V6Object();
      V6Value result = objValue(copy);
      seen.put(ref, result);
      V6Array keys = src.enumKeys();
      for (int i = 0; i < keys.elemCount; i++) {
        String key = keys.elements[i].toString();
        copy.set(key, cloneInternal(src.get(key), seen));
      }
      return result;
    }

    throw new V6Throw(str("DataCloneError: value could not be cloned"));
  }
}
