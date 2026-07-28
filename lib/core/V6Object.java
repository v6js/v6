import java.util.LinkedHashMap;
import java.util.Map;

public class V6Object {
  protected final Map<String, V6Value> props = new LinkedHashMap<>();
  protected int length = 0;
  private V6Object proto;

  public void setProto(V6Object proto) {
    this.proto = proto;
  }

  public V6Object getProto() {
    return proto;
  }

  public V6Value get(String key) {
    if (key.equals("length"))
      return new V6Value(V6Value.TAG_NUM, length, null);
    V6Value v = props.get(key);
    if (v != null)
      return v;
    if (proto != null)
      return proto.get(key);
    return new V6Value(V6Value.TAG_UNDEF, 0, null);
  }

  public void set(String key, V6Value value) {
    props.put(key, value);
    int idx = parseIndex(key);
    if (idx >= 0 && idx + 1 > length)
      length = idx + 1;
  }

  public void push(V6Value value) {
    set(Integer.toString(length), value);
  }

  public V6Array enumKeys() {
    java.util.LinkedHashSet<String> seen = new java.util.LinkedHashSet<>();
    for (V6Object o = this; o != null; o = o.proto)
      seen.addAll(o.props.keySet());
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
    for (Map.Entry<String, V6Value> e : props.entrySet()) {
      if (!first)
        sb.append(", ");
      first = false;
      sb.append(e.getKey()).append(": ").append(e.getValue().toString());
    }
    return sb.append("}").toString();
  }
}
