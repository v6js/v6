public final class V6AssertFunction extends V6Object implements V6Callable {
  @Override
  public V6Value call(V6Value thisArg, V6Value[] args) {
    return get("ok").asCallable().call(thisArg, args);
  }
}
