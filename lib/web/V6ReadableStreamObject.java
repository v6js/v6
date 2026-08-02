import java.util.ArrayDeque;

public final class V6ReadableStreamObject extends V6Object {
  final ArrayDeque<V6Value> queue = new ArrayDeque<>();
  final ArrayDeque<V6Promise> pendingReads = new ArrayDeque<>();
  boolean closed = false;
  boolean errored = false;
  V6Value errorReason = new V6Value(V6Value.TAG_UNDEF, 0, null);
  boolean locked = false;
  V6Callable pullFn;
  V6Callable cancelFn;
  V6Object controller;
  final V6Promise closedPromise = new V6Promise();

  private static V6Value objValue(V6Object o) {
    return new V6Value(V6Value.TAG_OBJ, 0, o);
  }

  private static V6Value bool(boolean b) {
    return new V6Value(V6Value.TAG_BOOL, b ? 1 : 0, null);
  }

  private static final V6Value UNDEF = new V6Value(V6Value.TAG_UNDEF, 0, null);

  V6Value resultObj(V6Value value, boolean done) {
    V6Object o = new V6Object();
    o.set("value", value);
    o.set("done", bool(done));
    return objValue(o);
  }

  public void enqueue(V6Value chunk) {
    if (closed || errored)
      return;
    if (!pendingReads.isEmpty()) {
      pendingReads.poll().resolve(resultObj(chunk, false));
    } else {
      queue.add(chunk);
    }
  }

  public void closeStream() {
    if (closed || errored)
      return;
    closed = true;
    while (!pendingReads.isEmpty())
      pendingReads.poll().resolve(resultObj(UNDEF, true));
    closedPromise.resolve(UNDEF);
  }

  public void errorStream(V6Value reason) {
    if (closed || errored)
      return;
    errored = true;
    errorReason = reason;
    while (!pendingReads.isEmpty())
      pendingReads.poll().reject(reason);
    closedPromise.reject(reason);
  }

  public V6Promise read() {
    if (errored)
      return V6Promise.rejected(errorReason);
    if (!queue.isEmpty())
      return V6Promise.resolved(resultObj(queue.poll(), false));
    if (closed)
      return V6Promise.resolved(resultObj(UNDEF, true));
    V6Promise p = new V6Promise();
    pendingReads.add(p);
    if (pullFn != null)
      pullFn.call(UNDEF, new V6Value[] {objValue(controller)});
    return p;
  }
}
