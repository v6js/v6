public final class V6WorkerConstructor extends V6Object implements V6NativeConstructor {
  public static final V6Object PROTOTYPE = new V6Object();

  public V6WorkerConstructor() {
    set("prototype", new V6Value(V6Value.TAG_OBJ, 0, PROTOTYPE));
  }

  @Override
  public V6Value construct(V6Value[] args) {
    throw new V6Throw(new V6Value(
        V6Value.TAG_STR, 0,
        "worker_threads.Worker is not supported: this runtime has no way to "
            + "compile and run a second script from within a running program "
            + "(no runtime-accessible compiler, and the AOT-jar execution mode "
            + "embeds only the one already-compiled program)"));
  }

  @Override
  public V6Object prototypeObject() {
    return PROTOTYPE;
  }
}
