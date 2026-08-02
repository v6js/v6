import java.nio.charset.StandardCharsets;

public final class V6TextEncoderStreamConstructor
    extends V6Object implements V6NativeConstructor {
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
  public V6Value construct(V6Value[] args) {
    V6Object transformer = new V6Object();
    transformer.set("transform", fn((t, a) -> {
                      String chunk = V6Value.argAt(a, 0).toString();
                      V6Object controller = (V6Object)V6Value.argAt(a, 1).ref();
                      byte[] bytes = chunk.getBytes(StandardCharsets.UTF_8);
                      controller.get("enqueue").asCallable().call(
                          objValue(controller),
                          new V6Value[] {objValue(new V6Buffer(bytes))});
                      return UNDEF;
                    }));

    V6Value result = new V6TransformStreamConstructor().construct(
        new V6Value[] {objValue(transformer)});
    V6Object resultObj = (V6Object)result.ref();
    resultObj.defineGetter("encoding", (t, a) -> str("utf-8"));
    return result;
  }
}
