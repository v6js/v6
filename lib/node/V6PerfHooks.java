import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public final class V6PerfHooks {
  private V6PerfHooks() {
  }

  private static final long START_NANO = System.nanoTime();
  private static final long START_EPOCH_MS = System.currentTimeMillis();
  private static V6Object performanceObj = null;
  private static final InheritableThreadLocal<List<V6Object>> ENTRIES =
      new InheritableThreadLocal<>();

  private static List<V6Object> entries() {
    List<V6Object> e = ENTRIES.get();
    if (e == null) {
      e = Collections.synchronizedList(new ArrayList<>());
      ENTRIES.set(e);
    }
    return e;
  }

  private static V6Value num(double n) {
    return new V6Value(V6Value.TAG_NUM, n, null);
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

  private static double nowMs() {
    return (System.nanoTime() - START_NANO) / 1_000_000.0;
  }

  private static V6Object makeEntry(String name, String entryType,
                                    double startTime, double duration) {
    V6Object e = new V6Object();
    e.set("name", str(name));
    e.set("entryType", str(entryType));
    e.set("startTime", num(startTime));
    e.set("duration", num(duration));
    return e;
  }

  private static double findMarkTime(List<V6Object> entries, String name) {
    for (int i = entries.size() - 1; i >= 0; i--) {
      V6Object e = entries.get(i);
      if (e.get("entryType").toString().equals("mark") &&
          e.get("name").toString().equals(name))
        return e.get("startTime").toNumber();
    }
    throw new V6Throw(str("SyntaxError: no mark named " + name));
  }

  private static V6Value toArray(List<V6Object> list) {
    V6Array arr = new V6Array();
    for (V6Object e : list)
      arr.push(objValue(e));
    return objValue(arr);
  }

  public static V6Object buildPerformanceObject() {
    V6Object performance = new V6Object();

    performance.set("now", fn((t, a) -> num(nowMs())));
    performance.set("timeOrigin", num(START_EPOCH_MS));

    performance.set("mark", fn((t, a) -> {
                      String name = V6Value.argAt(a, 0).toString();
                      V6Object e = makeEntry(name, "mark", nowMs(), 0);
                      entries().add(e);
                      return objValue(e);
                    }));

    performance.set(
        "measure", fn((t, a) -> {
          String name = V6Value.argAt(a, 0).toString();
          V6Value startArg = V6Value.argAt(a, 1);
          V6Value endArg = V6Value.argAt(a, 2);
          double startTime = startArg.tag() == V6Value.TAG_STR
                                 ? findMarkTime(entries(), startArg.toString())
                             : startArg.isUndefined() ? 0
                                                      : startArg.toNumber();
          double endTime = endArg.tag() == V6Value.TAG_STR
                               ? findMarkTime(entries(), endArg.toString())
                           : endArg.isUndefined() ? nowMs()
                                                  : endArg.toNumber();
          V6Object e =
              makeEntry(name, "measure", startTime, endTime - startTime);
          entries().add(e);
          return objValue(e);
        }));

    performance.set("getEntries",
                    fn((t, a) -> toArray(new ArrayList<>(entries()))));

    performance.set(
        "getEntriesByName", fn((t, a) -> {
          String name = V6Value.argAt(a, 0).toString();
          V6Value typeArg = V6Value.argAt(a, 1);
          String type = typeArg.isUndefined() ? null : typeArg.toString();
          List<V6Object> out = new ArrayList<>();
          for (V6Object e : entries())
            if (e.get("name").toString().equals(name) &&
                (type == null || e.get("entryType").toString().equals(type)))
              out.add(e);
          return toArray(out);
        }));

    performance.set("getEntriesByType", fn((t, a) -> {
                      String type = V6Value.argAt(a, 0).toString();
                      List<V6Object> out = new ArrayList<>();
                      for (V6Object e : entries())
                        if (e.get("entryType").toString().equals(type))
                          out.add(e);
                      return toArray(out);
                    }));

    performance.set(
        "clearMarks", fn((t, a) -> {
          V6Value nameArg = V6Value.argAt(a, 0);
          String name = nameArg.isUndefined() ? null : nameArg.toString();
          entries().removeIf(
              e
              -> e.get("entryType").toString().equals("mark") &&
                     (name == null || e.get("name").toString().equals(name)));
          return UNDEF;
        }));

    performance.set(
        "clearMeasures", fn((t, a) -> {
          V6Value nameArg = V6Value.argAt(a, 0);
          String name = nameArg.isUndefined() ? null : nameArg.toString();
          entries().removeIf(
              e
              -> e.get("entryType").toString().equals("measure") &&
                     (name == null || e.get("name").toString().equals(name)));
          return UNDEF;
        }));

    return performance;
  }

  public static V6Object build() {
    performanceObj = buildPerformanceObject();
    V6Object o = new V6Object();
    o.set("performance", objValue(performanceObj));
    return o;
  }

  public static V6Value performance() {
    return objValue(performanceObj != null ? performanceObj
                                           : buildPerformanceObject());
  }

  public static final V6Value MODULE = objValue(build());
}
