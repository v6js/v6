import java.io.IOException;
import java.util.concurrent.atomic.AtomicInteger;

public final class V6Cluster {
  private V6Cluster() {
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

  private static V6Value num(double n) {
    return new V6Value(V6Value.TAG_NUM, n, null);
  }

  private static V6Value bool(boolean b) {
    return new V6Value(V6Value.TAG_BOOL, b ? 1 : 0, null);
  }

  private static final V6Value UNDEF = new V6Value(V6Value.TAG_UNDEF, 0, null);
  private static final AtomicInteger nextWorkerId = new AtomicInteger(1);

  public static V6Object build() {
    boolean isWorker = System.getenv("V6_CLUSTER_WORKER") != null;
    V6EventEmitterObject o = new V6EventEmitterObject();
    o.setProto(V6EventEmitterConstructor.PROTOTYPE);

    o.set("isPrimary", bool(!isWorker));
    o.set("isMaster", bool(!isWorker));
    o.set("isWorker", bool(isWorker));

    V6Object workersObj = new V6Object();
    o.set("workers", objValue(workersObj));

    if (isWorker) {
      String idStr = System.getenv("V6_CLUSTER_WORKER_ID");
      V6Object workerSelf = new V6Object();
      workerSelf.set("id", num(idStr != null ? Double.parseDouble(idStr) : 1));
      o.set("worker", objValue(workerSelf));
    } else {
      o.set("worker", UNDEF);
    }

    o.set(
        "fork", fn((thisArg, args) -> {
          String exePath = V6IpcUtil.detectV6ExecutablePath();
          V6EventEmitterObject workerHandle = new V6EventEmitterObject();
          workerHandle.setProto(V6EventEmitterConstructor.PROTOTYPE);
          int id = nextWorkerId.getAndIncrement();
          workerHandle.set("id", num(id));

          if (exePath == null) {
            V6MicrotaskQueue.enqueue(
                ()
                    -> workerHandle.get("emit").asCallable().call(
                        objValue(workerHandle),
                        new V6Value[] {
                            str("error"),
                            str("cluster.fork is not supported when running "
                                + "via "
                                + "'java -jar' (no compiler available to run a "
                                + "second script)")}));
            return objValue(workerHandle);
          }
          if (V6Process.scriptPath.isEmpty()) {
            V6MicrotaskQueue.enqueue(
                ()
                    -> workerHandle.get("emit").asCallable().call(
                        objValue(workerHandle),
                        new V6Value[] {
                            str("error"),
                            str("cluster.fork: could not determine the current "
                                + "script's path")}));
            return objValue(workerHandle);
          }

          ProcessBuilder pb = new ProcessBuilder(exePath, "--no-daemon",
                                                 V6Process.scriptPath);
          pb.environment().put("V6_CLUSTER_WORKER", "1");
          pb.environment().put("V6_CLUSTER_WORKER_ID", String.valueOf(id));

          Process proc;
          try {
            proc = pb.start();
          } catch (IOException e) {
            V6MicrotaskQueue.enqueue(
                ()
                    -> workerHandle.get("emit").asCallable().call(
                        objValue(workerHandle),
                        new V6Value[] {str("error"),
                                       str(String.valueOf(e.getMessage()))}));
            return objValue(workerHandle);
          }

          final Process fproc = proc;
          workerHandle.set("send", fn((t, a) -> {
                             V6IpcUtil.sendMessage(fproc.getOutputStream(),
                                                   V6Value.argAt(a, 0));
                             return bool(true);
                           }));
          workerHandle.set("kill", fn((t, a) -> {
                             fproc.destroy();
                             return bool(true);
                           }));

          V6IpcUtil.pumpMessages(
              proc.getInputStream(), System.out,
              msg
              -> workerHandle.get("emit").asCallable().call(
                  objValue(workerHandle), new V6Value[] {str("message"), msg}));
          V6IpcUtil.pumpMessages(proc.getErrorStream(), System.err, msg -> {});

          workersObj.set(String.valueOf(id), objValue(workerHandle));

          V6EventLoop.ref();
          Thread waitThread = new Thread(() -> {
            try {
              int code = fproc.waitFor();
              V6EventLoop.postExternal(() -> {
                workerHandle.get("emit").asCallable().call(
                    objValue(workerHandle),
                    new V6Value[] {str("exit"), num(code)});
                o.get("emit").asCallable().call(
                    objValue(o),
                    new V6Value[] {str("exit"), objValue(workerHandle),
                                   num(code)});
                workersObj.delete(String.valueOf(id));
              });
            } catch (InterruptedException ignored) {
            } finally {
              V6EventLoop.unref();
            }
          });
          waitThread.setDaemon(true);
          waitThread.start();

          return objValue(workerHandle);
        }));

    o.set("disconnect", fn((thisArg, args) -> UNDEF));

    return o;
  }
}
