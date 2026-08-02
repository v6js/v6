import java.io.IOException;

public final class V6WorkerConstructor
    extends V6Object implements V6NativeConstructor {
  public static final V6Object PROTOTYPE = new V6Object();
  private static final V6Value UNDEF = new V6Value(V6Value.TAG_UNDEF, 0, null);

  public V6WorkerConstructor() {
    set("prototype", new V6Value(V6Value.TAG_OBJ, 0, PROTOTYPE));
    PROTOTYPE.setProto(V6EventEmitterConstructor.PROTOTYPE);
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

  @Override
  public V6Value construct(V6Value[] args) {
    String scriptPath = V6Value.argAt(args, 0).toString();
    V6Value optionsVal = V6Value.argAt(args, 1);
    V6Value workerData = optionsVal.tag() == V6Value.TAG_OBJ
                             ? ((V6Object)optionsVal.ref()).get("workerData")
                             : UNDEF;

    V6EventEmitterObject worker = new V6EventEmitterObject();
    worker.setProto(PROTOTYPE);

    String exePath = V6IpcUtil.detectV6ExecutablePath();
    if (exePath == null) {
      throw new V6Throw(str(
          "worker_threads.Worker is not supported when running via 'java -jar' "
          +
          "(AOT-jar mode embeds only the one already-compiled program, with no "
          + "compiler available to run a second script); run via the v6 "
          + "executable "
          + "directly to use Worker"));
    }

    ProcessBuilder pb = new ProcessBuilder(exePath, scriptPath);
    pb.environment().put("V6_WORKER_MODE", "1");
    if (!workerData.isUndefined())
      pb.environment().put(
          "V6_WORKER_DATA",
          V6Json.stringify(workerData, UNDEF, UNDEF).toString());

    Process proc;
    try {
      proc = pb.start();
    } catch (IOException e) {
      V6MicrotaskQueue.enqueue(
          ()
              -> worker.get("emit").asCallable().call(
                  objValue(worker),
                  new V6Value[] {str("error"),
                                 str(String.valueOf(e.getMessage()))}));
      return objValue(worker);
    }

    final Process fproc = proc;
    worker.set("postMessage", fn((t, a) -> {
                 V6IpcUtil.sendMessage(fproc.getOutputStream(),
                                       V6Value.argAt(a, 0));
                 return UNDEF;
               }));
    worker.set("terminate", fn((t, a) -> {
                 fproc.destroy();
                 return UNDEF;
               }));

    V6IpcUtil.pumpMessages(
        proc.getInputStream(), System.out,
        msg
        -> worker.get("emit").asCallable().call(
            objValue(worker), new V6Value[] {str("message"), msg}));
    V6IpcUtil.pumpMessages(proc.getErrorStream(), System.err, msg -> {});

    V6EventLoop.ref();
    Thread waitThread = new Thread(() -> {
      try {
        int code = proc.waitFor();
        V6EventLoop.postExternal(
            ()
                -> worker.get("emit").asCallable().call(
                    objValue(worker), new V6Value[] {str("exit"), num(code)}));
      } catch (InterruptedException ignored) {
      } finally {
        V6EventLoop.unref();
      }
    });
    waitThread.setDaemon(true);
    waitThread.start();

    return objValue(worker);
  }

  @Override
  public V6Object prototypeObject() {
    return PROTOTYPE;
  }
}
