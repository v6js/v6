import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.CopyOnWriteArrayList;

public final class V6DiagnosticsChannel {
  private V6DiagnosticsChannel() {
  }

  private static V6Value str(String s) {
    return new V6Value(V6Value.TAG_STR, 0, s);
  }

  private static V6Value fn(V6Callable c) {
    return new V6Value(V6Value.TAG_FUNC, 0, c);
  }

  private static V6Value objValue(V6Object o) {
    return new V6Value(V6Value.TAG_OBJ, 0, o);
  }

  private static final V6Value UNDEF = new V6Value(V6Value.TAG_UNDEF, 0, null);

  private static final InheritableThreadLocal<Map<String, V6Object>> CHANNELS_TL =
      new InheritableThreadLocal<>();
  private static final InheritableThreadLocal<Map<String, List<V6Callable>>>
      SUBSCRIBERS_TL = new InheritableThreadLocal<>();

  private static Map<String, V6Object> channels() {
    Map<String, V6Object> m = CHANNELS_TL.get();
    if (m == null) {
      m = new ConcurrentHashMap<>();
      CHANNELS_TL.set(m);
    }
    return m;
  }

  private static Map<String, List<V6Callable>> subscribersMap() {
    Map<String, List<V6Callable>> m = SUBSCRIBERS_TL.get();
    if (m == null) {
      m = new ConcurrentHashMap<>();
      SUBSCRIBERS_TL.set(m);
    }
    return m;
  }

  private static List<V6Callable> subscribersOf(String name) {
    return subscribersMap().computeIfAbsent(name,
                                            k -> new CopyOnWriteArrayList<>());
  }

  private static V6Object channelFor(String name) {
    return channels().computeIfAbsent(name, n -> {
      V6Object ch = new V6Object();
      ch.defineGetter("hasSubscribers",
                      (t, a)
                          -> new V6Value(V6Value.TAG_BOOL,
                                         subscribersOf(n).isEmpty() ? 0 : 1,
                                         null));
      ch.set("publish", fn((t, a) -> {
               V6Value message = V6Value.argAt(a, 0);
               for (V6Callable sub : subscribersOf(n))
                 sub.call(UNDEF, new V6Value[] {message, str(n)});
               return UNDEF;
             }));
      ch.set("subscribe", fn((t, a) -> {
               subscribersOf(n).add(V6Value.argAt(a, 0).asCallable());
               return UNDEF;
             }));
      ch.set("unsubscribe", fn((t, a) -> {
               V6Callable target = V6Value.argAt(a, 0).asCallable();
               boolean removed = subscribersOf(n).remove(target);
               return new V6Value(V6Value.TAG_BOOL, removed ? 1 : 0, null);
             }));
      return ch;
    });
  }

  public static V6Object build() {
    V6Object o = new V6Object();

    o.set(
        "channel",
        fn((thisArg,
            args) -> objValue(channelFor(V6Value.argAt(args, 0).toString()))));

    o.set("hasSubscribers", fn((thisArg, args) -> {
            String name = V6Value.argAt(args, 0).toString();
            return new V6Value(V6Value.TAG_BOOL,
                               subscribersOf(name).isEmpty() ? 0 : 1, null);
          }));

    o.set("subscribe", fn((thisArg, args) -> {
            String name = V6Value.argAt(args, 0).toString();
            subscribersOf(name).add(V6Value.argAt(args, 1).asCallable());
            return UNDEF;
          }));

    o.set("unsubscribe", fn((thisArg, args) -> {
            String name = V6Value.argAt(args, 0).toString();
            boolean removed =
                subscribersOf(name).remove(V6Value.argAt(args, 1).asCallable());
            return new V6Value(V6Value.TAG_BOOL, removed ? 1 : 0, null);
          }));

    return o;
  }
}
