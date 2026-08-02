import java.util.ArrayDeque;

public final class V6WritableStreamObject extends V6Object {
  final ArrayDeque<V6Value> chunkQueue = new ArrayDeque<>();
  final ArrayDeque<V6Promise> resultQueue = new ArrayDeque<>();
  boolean writing = false;
  boolean closed = false;
  boolean errored = false;
  V6Value errorReason = new V6Value(V6Value.TAG_UNDEF, 0, null);
  boolean locked = false;
  V6Callable writeFn;
  V6Callable closeFn;
  V6Callable abortFn;
  V6Object controller;
  final V6Promise closedPromise = new V6Promise();

  private static V6Value objValue(V6Object o) {
    return new V6Value(V6Value.TAG_OBJ, 0, o);
  }

  private static final V6Value UNDEF = new V6Value(V6Value.TAG_UNDEF, 0, null);

  public V6Promise write(V6Value chunk) {
    V6Promise p = new V6Promise();
    if (errored) {
      p.reject(errorReason);
      return p;
    }
    chunkQueue.add(chunk);
    resultQueue.add(p);
    processQueue();
    return p;
  }

  private void processQueue() {
    if (writing || chunkQueue.isEmpty())
      return;
    writing = true;
    V6Value chunk = chunkQueue.poll();
    V6Promise resultP = resultQueue.poll();
    V6Value writeResult =
        writeFn != null
            ? writeFn.call(UNDEF, new V6Value[] {chunk, objValue(controller)})
            : UNDEF;
    V6Promise wp = V6Promise.resolved(writeResult);
    wp.addCallbacks(
        (okVal)
            -> {
          resultP.resolve(UNDEF);
          writing = false;
          processQueue();
        },
        (errVal) -> {
          resultP.reject(errVal);
          errored = true;
          errorReason = errVal;
          writing = false;
        });
  }

  public V6Promise close() {
    V6Promise p = new V6Promise();
    if (closeFn != null) {
      V6Value r = closeFn.call(UNDEF, new V6Value[] {objValue(controller)});
      V6Promise.resolved(r).addCallbacks(
          (okVal)
              -> {
            closed = true;
            closedPromise.resolve(UNDEF);
            p.resolve(UNDEF);
          },
          (errVal) -> {
            errored = true;
            errorReason = errVal;
            closedPromise.reject(errVal);
            p.reject(errVal);
          });
    } else {
      closed = true;
      closedPromise.resolve(UNDEF);
      p.resolve(UNDEF);
    }
    return p;
  }

  public V6Promise abort(V6Value reason) {
    V6Promise p = new V6Promise();
    errored = true;
    errorReason = reason;
    if (abortFn != null) {
      V6Value r = abortFn.call(UNDEF, new V6Value[] {reason});
      V6Promise.resolved(r).addCallbacks(
          (okVal) -> p.resolve(UNDEF), (errVal) -> p.reject(errVal));
    } else {
      p.resolve(UNDEF);
    }
    closedPromise.reject(reason);
    return p;
  }
}
