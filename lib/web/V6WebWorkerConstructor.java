import java.io.IOException;

public final class V6WebWorkerConstructor
    extends V6Object implements V6NativeConstructor {
  public static final V6Object PROTOTYPE = buildPrototype();

  public V6WebWorkerConstructor() {
    set("prototype", new V6Value(V6Value.TAG_OBJ, 0, PROTOTYPE));
    PROTOTYPE.nativeCtor = this;
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

  @Override
  public V6Object allocate() {
    return new V6WebWorkerObject();
  }

  @Override
  public V6Value construct(V6Value[] args) {
    String scriptPath = V6Value.argAt(args, 0).toString();

    V6WebWorkerObject worker = new V6WebWorkerObject();
    worker.setProto(PROTOTYPE);

    String exePath = V6IpcUtil.detectV6ExecutablePath();
    if (exePath == null)
      throw new V6Throw(str(
          "Worker is not supported when running via 'java -jar' (AOT-jar mode "
          + "embeds only the one already-compiled program, with no compiler "
          + "available to run a second script); run via the v6 executable "
          + "directly to use Worker"));

    ProcessBuilder pb = new ProcessBuilder(exePath, scriptPath);
    pb.environment().put("V6_WORKER_MODE", "1");

    Process proc;
    try {
      proc = pb.start();
    } catch (IOException e) {
      V6MicrotaskQueue.enqueue(() -> {
        V6EventObject ev = new V6EventObject();
        ev.setProto(V6EventConstructor.PROTOTYPE);
        ev.type = "error";
        ev.set("message", str(String.valueOf(e.getMessage())));
        worker.dispatch(ev);
      });
      return objValue(worker);
    }
    worker.proc = proc;

    worker.set("postMessage", fn((t, a) -> {
                 V6IpcUtil.sendMessage(proc.getOutputStream(),
                                       V6Value.argAt(a, 0));
                 return UNDEF;
               }));
    worker.set("terminate", fn((t, a) -> {
                 proc.destroy();
                 return UNDEF;
               }));

    V6IpcUtil.pumpMessages(proc.getInputStream(), System.out, msg -> {
      V6MessageEventObject ev = new V6MessageEventObject();
      ev.setProto(V6MessageEventConstructor.PROTOTYPE);
      ev.type = "message";
      ev.data = msg;
      worker.dispatch(ev);
    });
    V6IpcUtil.pumpMessages(proc.getErrorStream(), System.err, msg -> {});

    V6EventLoop.ref();
    Thread waitThread = new Thread(() -> {
      try {
        proc.waitFor();
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

  private static V6Object buildPrototype() {
    V6Object o = new V6Object();
    o.setProto(V6EventTargetConstructor.PROTOTYPE);
    V6EventHandlerProperty.install(o, "onmessage", "message");
    V6EventHandlerProperty.install(o, "onmessageerror", "messageerror");
    V6EventHandlerProperty.install(o, "onerror", "error");
    return o;
  }
}
