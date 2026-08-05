import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

public final class V6Shape {
  public static final V6Shape EMPTY =
      new V6Shape(null, null, 0, Collections.emptyMap());
  private static final int MAX_FAST_SLOTS = 24;

  final V6Shape parent;
  final String addedKey;
  final int slotCount;
  private final Map<String, Integer> slotIndex;
  private final ConcurrentHashMap<String, V6Shape> transitions =
      new ConcurrentHashMap<>(4);
  private volatile String[] orderedKeys;

  private V6Shape(V6Shape parent, String addedKey, int slotCount,
                  Map<String, Integer> slotIndex) {
    this.parent = parent;
    this.addedKey = addedKey;
    this.slotCount = slotCount;
    this.slotIndex = slotIndex;
  }

  public boolean isFull() {
    return slotCount >= MAX_FAST_SLOTS;
  }

  public V6Shape transition(String key) {
    V6Shape cached = transitions.get(key);
    if (cached != null)
      return cached;
    return transitions.computeIfAbsent(key, k -> {
      Map<String, Integer> next = new HashMap<>(slotIndex);
      next.put(k, slotCount);
      return new V6Shape(this, k, slotCount + 1, next);
    });
  }

  public String[] orderedKeys() {
    String[] keys = orderedKeys;
    if (keys != null)
      return keys;
    keys = new String[slotCount];
    for (Map.Entry<String, Integer> e : slotIndex.entrySet())
      keys[e.getValue()] = e.getKey();
    orderedKeys = keys;
    return keys;
  }

  public int slotOf(String key) {
    Integer slot = slotIndex.get(key);
    return slot != null ? slot : -1;
  }
}
