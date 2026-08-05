public final class V6Shape {
  public static final V6Shape EMPTY = new V6Shape(null, null, 0);
  private static final int MAX_FAST_SLOTS = 24;

  final V6Shape parent;
  final String addedKey;
  final int slotCount;
  private java.util.HashMap<String, V6Shape> transitions;
  private String[] orderedKeys;

  private V6Shape(V6Shape parent, String addedKey, int slotCount) {
    this.parent = parent;
    this.addedKey = addedKey;
    this.slotCount = slotCount;
  }

  public boolean isFull() {
    return slotCount >= MAX_FAST_SLOTS;
  }

  public synchronized V6Shape transition(String key) {
    if (transitions == null)
      transitions = new java.util.HashMap<>(4);
    V6Shape next = transitions.get(key);
    if (next != null)
      return next;
    next = new V6Shape(this, key, slotCount + 1);
    transitions.put(key, next);
    return next;
  }

  public String[] orderedKeys() {
    String[] keys = orderedKeys;
    if (keys != null)
      return keys;
    keys = new String[slotCount];
    V6Shape s = this;
    for (int i = slotCount - 1; i >= 0; i--) {
      keys[i] = s.addedKey;
      s = s.parent;
    }
    orderedKeys = keys;
    return keys;
  }

  public int slotOf(String key) {
    String[] keys = orderedKeys();
    for (int i = 0; i < keys.length; i++)
      if (keys[i].equals(key))
        return i;
    return -1;
  }
}
