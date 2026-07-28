import java.util.LinkedHashMap;
import java.util.Map;

public final class V6Object {
  private final Map<String, V6Value> props = new LinkedHashMap<>();
  private final boolean isArray;
  private int length = 0;

  public V6Object(boolean isArray) {
    this.isArray = isArray;
  }

  public V6Value get(String key) {
    if (key.equals("length"))
      return new V6Value(V6Value.TAG_NUM, length, null);
    V6Value v = props.get(key);
    return v == null ? new V6Value(V6Value.TAG_UNDEF, 0, null) : v;
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

  private static int parseIndex(String key) {
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
    if (isArray) {
      StringBuilder sb = new StringBuilder();
      for (int i = 0; i < length; i++) {
        if (i > 0)
          sb.append(",");
        V6Value v = get(Integer.toString(i));
        if (v.tag() != V6Value.TAG_UNDEF)
          sb.append(v.toString());
      }
      return sb.toString();
    }
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
