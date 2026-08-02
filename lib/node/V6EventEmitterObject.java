import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;

public final class V6EventEmitterObject extends V6Object {
  final LinkedHashMap<String, List<V6Value>> listeners = new LinkedHashMap<>();

  List<V6Value> listenersFor(String event, boolean create) {
    List<V6Value> list = listeners.get(event);
    if (list == null && create) {
      list = new ArrayList<>();
      listeners.put(event, list);
    }
    return list;
  }
}
