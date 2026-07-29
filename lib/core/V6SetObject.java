import java.util.LinkedHashMap;

public final class V6SetObject extends V6Object {
  final LinkedHashMap<Object, V6Value> entries = new LinkedHashMap<>();

  public V6SetObject() {
    length = 0;
  }
}
