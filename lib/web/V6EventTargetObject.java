import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

public class V6EventTargetObject extends V6Object {
  final V6EventEmitterObject store = new V6EventEmitterObject();
  final Set<V6Value> onceListeners = new HashSet<>();
  final Map<String, V6Value> handlerRaw = new HashMap<>();

  private static V6Value objValue(V6Object o) {
    return new V6Value(V6Value.TAG_OBJ, 0, o);
  }

  private void invokeListener(V6Value listener, V6EventObject event) {
    if (listener.tag() == V6Value.TAG_FUNC) {
      listener.asCallable().call(objValue(this),
                                 new V6Value[] {objValue(event)});
    } else if (listener.tag() == V6Value.TAG_OBJ && listener.ref() instanceof
                                                        V6Object) {
      V6Value handleEvent = ((V6Object)listener.ref()).get("handleEvent");
      if (handleEvent.tag() == V6Value.TAG_FUNC)
        handleEvent.asCallable().call(listener,
                                      new V6Value[] {objValue(event)});
    }
  }

  public boolean dispatch(V6EventObject event) {
    event.target = objValue(this);
    event.currentTarget = objValue(this);
    List<V6Value> list = store.listenersFor(event.type, false);
    if (list != null) {
      java.util.List<V6Value> snapshot = new java.util.ArrayList<>(list);
      for (V6Value listener : snapshot) {
        if (event.immediatePropagationStopped)
          break;
        invokeListener(listener, event);
        if (onceListeners.remove(listener))
          list.remove(listener);
      }
    }
    event.currentTarget = new V6Value(V6Value.TAG_NULL, 0, null);
    return !event.defaultPrevented;
  }

  public void addListener(String type, V6Value listener, boolean once,
                          V6Value signal) {
    if (listener == null || listener.isUndefined() ||
        listener.tag() == V6Value.TAG_NULL)
      return;
    List<V6Value> list = store.listenersFor(type, true);
    if (list.contains(listener))
      return;
    list.add(listener);
    if (once)
      onceListeners.add(listener);
    if (signal != null && signal.tag() == V6Value.TAG_OBJ &&
        signal.ref() instanceof V6Object) {
      V6Object sig = (V6Object)signal.ref();
      V6Value aborted = sig.get("aborted");
      V6Value addFn = sig.get("addEventListener");
      if (!(aborted.tag() == V6Value.TAG_BOOL && aborted.truthy()) &&
          addFn.tag() == V6Value.TAG_FUNC) {
        V6Value cleanup =
            new V6Value(V6Value.TAG_FUNC, 0, (V6Callable)(t, a) -> {
              removeListener(type, listener);
              return new V6Value(V6Value.TAG_UNDEF, 0, null);
            });
        addFn.asCallable().call(
            signal,
            new V6Value[] {new V6Value(V6Value.TAG_STR, 0, "abort"), cleanup});
      }
    }
  }

  public void removeListener(String type, V6Value listener) {
    List<V6Value> list = store.listenersFor(type, false);
    if (list != null)
      list.remove(listener);
    onceListeners.remove(listener);
  }
}
