import java.util.LinkedHashMap;
import java.util.Map;

public class V6Object {
  private static final V6Value[] EMPTY_ELEMENTS = new V6Value[0];
  private static final V6Value[] EMPTY_ARGS = new V6Value[0];

  protected final Map<String, V6Value> props = new LinkedHashMap<>();
  protected V6Value[] elements = EMPTY_ELEMENTS;
  protected int elemCount = 0;
  protected int length = 0;
  private V6Object proto;
  private boolean frozen = false;
  private boolean sealed = false;
  private Map<String, V6Callable> getters;
  private Map<String, V6Callable> setters;
  public V6Value newTarget;
  public V6NativeConstructor nativeCtor;

  public void defineGetter(String key, V6Callable getter) {
    if (getters == null)
      getters = new LinkedHashMap<>();
    getters.put(key, getter);
    props.remove(key);
  }

  public void defineSetter(String key, V6Callable setter) {
    if (setters == null)
      setters = new LinkedHashMap<>();
    setters.put(key, setter);
    props.remove(key);
  }

  private V6Callable findGetter(String key) {
    if (getters != null) {
      V6Callable g = getters.get(key);
      if (g != null)
        return g;
    }
    return proto != null ? proto.findGetter(key) : null;
  }

  private boolean hasAccessor(String key) {
    if ((getters != null && getters.containsKey(key)) ||
        (setters != null && setters.containsKey(key)))
      return true;
    return proto != null && proto.hasAccessor(key);
  }

  private V6Callable findSetter(String key) {
    if (setters != null) {
      V6Callable s = setters.get(key);
      if (s != null)
        return s;
    }
    return proto != null ? proto.findSetter(key) : null;
  }

  private void ensureCapacity(int min) {
    if (elements.length >= min)
      return;
    int newCap = Math.max(min, elements.length == 0 ? 8 : elements.length * 2);
    elements = java.util.Arrays.copyOf(elements, newCap);
  }

  public void setProto(V6Object proto) {
    this.proto = proto;
  }

  public void setProtoFromValue(V6Value v) {
    if (v.tag() == V6Value.TAG_OBJ)
      setProto((V6Object)v.ref());
  }

  public V6Object getProto() {
    return proto;
  }

  public void freeze() {
    frozen = true;
    sealed = true;
  }

  public boolean isFrozenFlag() {
    return frozen;
  }

  public void seal() {
    sealed = true;
  }

  public boolean isSealedFlag() {
    return sealed;
  }

  public V6Value get(String key) {
    return get(key, new V6Value(V6Value.TAG_OBJ, 0, this));
  }

  private V6Value get(String key, V6Value receiver) {
    if (key.equals("length") && !props.containsKey("length"))
      return new V6Value(V6Value.TAG_NUM, length, null);
    int idx = parseIndex(key);
    if (idx >= 0 && idx < elemCount)
      return elements[idx];
    if (hasAccessor(key)) {
      V6Callable g = findGetter(key);
      if (g == null)
        return new V6Value(V6Value.TAG_UNDEF, 0, null);
      return g.call(receiver, EMPTY_ARGS);
    }
    V6Value v = props.get(key);
    if (v != null)
      return v;
    if (proto != null)
      return proto.get(key, receiver);
    return new V6Value(V6Value.TAG_UNDEF, 0, null);
  }

  public boolean delete(String key) {
    int idx = parseIndex(key);
    if (idx >= 0 && idx < elemCount) {
      elements[idx] = new V6Value(V6Value.TAG_UNDEF, 0, null);
      return true;
    }
    props.remove(key);
    if (getters != null)
      getters.remove(key);
    if (setters != null)
      setters.remove(key);
    return true;
  }

  public boolean has(String key) {
    if (key.equals("length") && !props.containsKey("length"))
      return true;
    int idx = parseIndex(key);
    if (idx >= 0 && idx < elemCount)
      return true;
    if (props.containsKey(key))
      return true;
    return proto != null && proto.has(key);
  }

  public void set(String key, V6Value value) {
    if (hasAccessor(key)) {
      V6Callable s = findSetter(key);
      if (s != null)
        s.call(new V6Value(V6Value.TAG_OBJ, 0, this), new V6Value[] {value});
      return;
    }
    if (frozen)
      return;
    int idx = parseIndex(key);
    boolean isDenseAppend = idx == elemCount;
    boolean isDenseUpdate = idx >= 0 && idx < elemCount;
    if (sealed && !props.containsKey(key) && !isDenseUpdate)
      return;
    if (isDenseAppend) {
      ensureCapacity(elemCount + 1);
      elements[elemCount] = value;
      elemCount++;
      if (elemCount > length)
        length = elemCount;
      return;
    }
    if (isDenseUpdate) {
      elements[idx] = value;
      return;
    }
    props.put(key, value);
    if (idx >= 0 && idx + 1 > length)
      length = idx + 1;
  }

  public void push(V6Value value) {
    set(Integer.toString(length), value);
  }

  public V6Value pop() {
    if (length == 0)
      return new V6Value(V6Value.TAG_UNDEF, 0, null);
    length--;
    if (elemCount > length) {
      elemCount = length;
      return elements[elemCount];
    }
    String key = Integer.toString(length);
    V6Value v = props.remove(key);
    return v != null ? v : new V6Value(V6Value.TAG_UNDEF, 0, null);
  }

  public V6Value shift() {
    if (length == 0)
      return new V6Value(V6Value.TAG_UNDEF, 0, null);
    V6Value first = get("0");
    for (int i = 1; i < length; i++)
      set(Integer.toString(i - 1), get(Integer.toString(i)));
    length--;
    props.remove(Integer.toString(length));
    return first;
  }

  public int unshift(V6Value[] items) {
    int n = items.length;
    for (int i = length - 1; i >= 0; i--)
      set(Integer.toString(i + n), get(Integer.toString(i)));
    for (int i = 0; i < n; i++)
      set(Integer.toString(i), items[i]);
    return length;
  }

  public V6Array slice(int start, int end) {
    if (start < 0)
      start = Math.max(0, length + start);
    if (end < 0)
      end = Math.max(0, length + end);
    start = Math.min(start, length);
    end = Math.min(end, length);
    V6Array result = new V6Array();
    for (int i = start; i < end; i++)
      result.push(get(Integer.toString(i)));
    return result;
  }

  public int indexOf(V6Value target) {
    for (int i = 0; i < length; i++)
      if (V6Value.strictEquals(get(Integer.toString(i)), target))
        return i;
    return -1;
  }

  public String join(String sep) {
    StringBuilder sb = new StringBuilder();
    for (int i = 0; i < length; i++) {
      if (i > 0)
        sb.append(sep);
      V6Value v = get(Integer.toString(i));
      if (v.tag() != V6Value.TAG_UNDEF && v.tag() != V6Value.TAG_NULL)
        sb.append(v.toString());
    }
    return sb.toString();
  }

  public V6Array map(V6Callable fn) {
    return map(fn, new V6Value(V6Value.TAG_UNDEF, 0, null));
  }

  public V6Array map(V6Callable fn, V6Value thisArg) {
    V6Array result = new V6Array();
    for (int i = 0; i < length; i++) {
      V6Value el = get(Integer.toString(i));
      V6Value idx = new V6Value(V6Value.TAG_NUM, i, null);
      result.push(fn.call(thisArg, new V6Value[] {el, idx}));
    }
    return result;
  }

  public V6Array filter(V6Callable fn) {
    V6Array result = new V6Array();
    V6Value undef = new V6Value(V6Value.TAG_UNDEF, 0, null);
    for (int i = 0; i < length; i++) {
      V6Value el = get(Integer.toString(i));
      V6Value idx = new V6Value(V6Value.TAG_NUM, i, null);
      if (fn.call(undef, new V6Value[] {el, idx}).truthy())
        result.push(el);
    }
    return result;
  }

  public boolean some(V6Callable fn) {
    V6Value undef = new V6Value(V6Value.TAG_UNDEF, 0, null);
    for (int i = 0; i < length; i++) {
      V6Value el = get(Integer.toString(i));
      V6Value idx = new V6Value(V6Value.TAG_NUM, i, null);
      if (fn.call(undef, new V6Value[] {el, idx}).truthy())
        return true;
    }
    return false;
  }

  public boolean every(V6Callable fn) {
    V6Value undef = new V6Value(V6Value.TAG_UNDEF, 0, null);
    for (int i = 0; i < length; i++) {
      V6Value el = get(Integer.toString(i));
      V6Value idx = new V6Value(V6Value.TAG_NUM, i, null);
      if (!fn.call(undef, new V6Value[] {el, idx}).truthy())
        return false;
    }
    return true;
  }

  public V6Value find(V6Callable fn) {
    V6Value undef = new V6Value(V6Value.TAG_UNDEF, 0, null);
    for (int i = 0; i < length; i++) {
      V6Value el = get(Integer.toString(i));
      V6Value idx = new V6Value(V6Value.TAG_NUM, i, null);
      if (fn.call(undef, new V6Value[] {el, idx}).truthy())
        return el;
    }
    return new V6Value(V6Value.TAG_UNDEF, 0, null);
  }

  public int findIndex(V6Callable fn) {
    V6Value undef = new V6Value(V6Value.TAG_UNDEF, 0, null);
    for (int i = 0; i < length; i++) {
      V6Value el = get(Integer.toString(i));
      V6Value idx = new V6Value(V6Value.TAG_NUM, i, null);
      if (fn.call(undef, new V6Value[] {el, idx}).truthy())
        return i;
    }
    return -1;
  }

  public void forEach(V6Callable fn) {
    V6Value undef = new V6Value(V6Value.TAG_UNDEF, 0, null);
    for (int i = 0; i < length; i++) {
      V6Value el = get(Integer.toString(i));
      V6Value idx = new V6Value(V6Value.TAG_NUM, i, null);
      fn.call(undef, new V6Value[] {el, idx});
    }
  }

  public V6Value reduce(V6Callable fn, V6Value[] args) {
    V6Value undef = new V6Value(V6Value.TAG_UNDEF, 0, null);
    int start = 0;
    V6Value acc;
    if (args.length >= 2) {
      acc = args[1];
    } else {
      if (length == 0)
        throw new RuntimeException(
            "Reduce of empty array with no initial value");
      acc = get("0");
      start = 1;
    }
    for (int i = start; i < length; i++) {
      V6Value el = get(Integer.toString(i));
      acc = fn.call(undef, new V6Value[] {
                               acc, el, new V6Value(V6Value.TAG_NUM, i, null)});
    }
    return acc;
  }

  public V6Array concatValues(V6Value[] items) {
    V6Array result = new V6Array();
    for (int i = 0; i < length; i++)
      result.push(get(Integer.toString(i)));
    for (V6Value v : items) {
      if (v.tag() == V6Value.TAG_OBJ && v.ref() instanceof V6Array)
        result.pushAll(v);
      else
        result.push(v);
    }
    return result;
  }

  public V6Object reverseInPlace() {
    for (int i = 0, j = length - 1; i < j; i++, j--) {
      V6Value tmp = get(Integer.toString(i));
      set(Integer.toString(i), get(Integer.toString(j)));
      set(Integer.toString(j), tmp);
    }
    return this;
  }

  public V6Object sortDefault() {
    V6Value[] arr = toValueArray();
    java.util.Arrays.sort(arr, (a, b) -> a.toString().compareTo(b.toString()));
    for (int i = 0; i < arr.length; i++)
      set(Integer.toString(i), arr[i]);
    return this;
  }

  public V6Object sortWith(V6Callable cmp) {
    V6Value[] arr = toValueArray();
    V6Value undef = new V6Value(V6Value.TAG_UNDEF, 0, null);
    java.util.Arrays.sort(arr, (a, b) -> {
      double d = cmp.call(undef, new V6Value[] {a, b}).toNumber();
      return d < 0 ? -1 : (d > 0 ? 1 : 0);
    });
    for (int i = 0; i < arr.length; i++)
      set(Integer.toString(i), arr[i]);
    return this;
  }

  public static V6Array restFromArgs(V6Value[] args, int start) {
    V6Array result = new V6Array();
    for (int i = start; i < args.length; i++)
      result.push(args[i]);
    return result;
  }

  public V6Array restFrom(int start) {
    V6Array result = new V6Array();
    for (int i = start; i < length; i++)
      result.push(get(Integer.toString(i)));
    return result;
  }

  public void pushAll(V6Value v) {
    if (v.tag() != V6Value.TAG_OBJ && v.tag() != V6Value.TAG_STR)
      return;
    V6Iterator it = new V6Iterator(v);
    while (it.hasNext())
      push(it.next());
  }

  public void spreadFrom(V6Value v) {
    if (v.tag() == V6Value.TAG_OBJ) {
      V6Object o = (V6Object)v.ref();
      for (int i = 0; i < o.elemCount; i++)
        set(Integer.toString(i), o.elements[i]);
      for (Map.Entry<String, V6Value> e : o.props.entrySet())
        set(e.getKey(), e.getValue());
    }
  }

  public V6Value[] toValueArray() {
    V6Value[] result = new V6Value[length];
    for (int i = 0; i < length; i++)
      result[i] = get(Integer.toString(i));
    return result;
  }

  public V6Array enumKeys() {
    java.util.LinkedHashSet<String> seen = new java.util.LinkedHashSet<>();
    for (V6Object o = this; o != null; o = o.proto) {
      for (int i = 0; i < o.elemCount; i++)
        seen.add(Integer.toString(i));
      seen.addAll(o.props.keySet());
    }
    V6Array arr = new V6Array();
    for (String k : seen)
      arr.push(new V6Value(V6Value.TAG_STR, 0, k));
    return arr;
  }

  static int parseIndex(String key) {
    if (key.isEmpty())
      return -1;
    for (int i = 0; i < key.length(); i++)
      if (!Character.isDigit(key.charAt(i)))
        return -1;
    try {
      return Integer.parseInt(key);
    } catch (NumberFormatException e) {
      return -1;
    }
  }

  @Override
  public String toString() {
    StringBuilder sb = new StringBuilder("{");
    boolean first = true;
    for (int i = 0; i < elemCount; i++) {
      if (!first)
        sb.append(", ");
      first = false;
      sb.append(i).append(": ").append(elements[i].toString());
    }
    for (Map.Entry<String, V6Value> e : props.entrySet()) {
      if (!first)
        sb.append(", ");
      first = false;
      sb.append(e.getKey()).append(": ").append(e.getValue().toString());
    }
    return sb.append("}").toString();
  }
}
