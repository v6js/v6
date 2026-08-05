public final class V6SparseModules {
  static V6Value fn(V6Callable c) {
    return V6Builtins.fn(c);
  }

  static V6Value objValue(V6Object o) {
    return V6Builtins.objValue(o);
  }

  private static V6Value buildStreamModule() {
    V6Object o = new V6Object();
    o.set("Readable",
          new V6Value(V6Value.TAG_OBJ, 0, new V6StreamReadableConstructor()));
    o.set("Writable",
          new V6Value(V6Value.TAG_OBJ, 0, new V6StreamWritableConstructor()));
    o.set("Duplex",
          new V6Value(V6Value.TAG_OBJ, 0, new V6StreamDuplexConstructor()));
    V6StreamTransformConstructor transformCtor =
        new V6StreamTransformConstructor();
    o.set("Transform", new V6Value(V6Value.TAG_OBJ, 0, transformCtor));
    o.set("PassThrough", new V6Value(V6Value.TAG_OBJ, 0, transformCtor));
    o.set("pipeline",
          fn((thisArg, args) -> V6StreamMethods.pipelineImpl(args)));
    o.set("finished",
          fn((thisArg, args) -> V6StreamMethods.finishedImpl(args)));
    V6Object promisesObj = new V6Object();
    promisesObj.set("pipeline", fn((thisArg, args) -> {
                      V6Promise p = new V6Promise();
                      java.util.List<V6Value> streams =
                          new java.util.ArrayList<>();
                      for (V6Value a : args)
                        if (a.tag() != V6Value.TAG_FUNC)
                          streams.add(a);
                      V6Value[] pipelineArgs = new V6Value[streams.size() + 1];
                      for (int i = 0; i < streams.size(); i++)
                        pipelineArgs[i] = streams.get(i);
                      pipelineArgs[streams.size()] = fn((t2, a2) -> {
                        V6Value err = V6Value.argAt(a2, 0);
                        if (err.tag() == V6Value.TAG_NULL || err.isUndefined())
                          p.resolve(new V6Value(V6Value.TAG_UNDEF, 0, null));
                        else
                          p.reject(err);
                        return new V6Value(V6Value.TAG_UNDEF, 0, null);
                      });
                      V6StreamMethods.pipelineImpl(pipelineArgs);
                      return objValue(p);
                    }));
    promisesObj.set(
        "finished", fn((thisArg, args) -> {
          V6Promise p = new V6Promise();
          V6StreamMethods.finishedImpl(new V6Value[] {
              V6Value.argAt(args, 0), fn((t2, a2) -> {
                V6Value err = V6Value.argAt(a2, 0);
                if (err.tag() == V6Value.TAG_NULL || err.isUndefined())
                  p.resolve(new V6Value(V6Value.TAG_UNDEF, 0, null));
                else
                  p.reject(err);
                return new V6Value(V6Value.TAG_UNDEF, 0, null);
              })});
          return objValue(p);
        }));
    o.set("promises", objValue(promisesObj));
    return objValue(o);
  }

  public static final V6Value NODE_STREAM = buildStreamModule();
  public static final V6Value NODE_CHILD_PROCESS =
      objValue(V6ChildProcess.build());
  public static final V6Value NODE_NET = objValue(V6Net.build());
  public static final V6Value NODE_HTTP = objValue(V6Http.build());
  public static final V6Value NODE_HTTPS = objValue(V6Http.buildHttps());
  public static final V6Value NODE_TLS = objValue(V6Tls.build());
  public static final V6Value NODE_READLINE = objValue(V6Readline.build());
  public static final V6Value NODE_WORKER_THREADS =
      objValue(V6WorkerThreads.build());
  public static final V6Value NODE_CLUSTER = objValue(V6Cluster.build());
  public static final V6Value NODE_REPL = objValue(V6Repl.build());
  public static final V6Value NODE_TIMERS = objValue(V6Timers.buildModule());
  public static final V6Value NODE_DGRAM = objValue(V6Dgram.build());
  public static final V6Value NODE_HTTP2 = objValue(V6Http2.build());
  public static final V6Value NODE_V8 = objValue(V6V8.build());
  public static final V6Value NODE_MODULE = objValue(V6ModuleModule.build());
  public static final V6Value NODE_DIAGNOSTICS_CHANNEL =
      objValue(V6DiagnosticsChannel.build());
  public static final V6Value NODE_ASYNC_HOOKS = objValue(V6AsyncHooks.build());
  public static final V6Value NODE_INSPECTOR = objValue(V6Inspector.build());
  public static final V6Value NODE_TRACE_EVENTS =
      objValue(V6TraceEvents.build());
}
