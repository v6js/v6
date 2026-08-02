import java.io.ByteArrayOutputStream;

public final class V6DecompressionStreamConstructor
    extends V6Object implements V6NativeConstructor {
  private static V6Value fn(V6Callable c) {
    return new V6Value(V6Value.TAG_FUNC, 0, c);
  }

  private static V6Value objValue(V6Object o) {
    return new V6Value(V6Value.TAG_OBJ, 0, o);
  }

  private static final V6Value UNDEF = new V6Value(V6Value.TAG_UNDEF, 0, null);

  private static byte[] bytesOf(V6Value v) {
    if (v.tag() == V6Value.TAG_OBJ && v.ref() instanceof V6Buffer)
      return ((V6Buffer)v.ref()).toBytes();
    return new byte[0];
  }

  @Override
  public V6Value construct(V6Value[] args) {
    String format = V6Value.argAt(args, 0).toString();
    ByteArrayOutputStream buf = new ByteArrayOutputStream();

    V6Object transformer = new V6Object();
    transformer.set("transform", fn((t, a) -> {
                      byte[] b = bytesOf(V6Value.argAt(a, 0));
                      buf.write(b, 0, b.length);
                      return UNDEF;
                    }));
    transformer.set("flush", fn((t, a) -> {
                      V6Object controller = (V6Object)V6Value.argAt(a, 0).ref();
                      byte[] input = buf.toByteArray();
                      byte[] output;
                      switch (format) {
                      case "gzip":
                        output = V6Zlib.gunzipBytes(input);
                        break;
                      case "deflate-raw":
                        output = V6Zlib.inflateBytes(input, true);
                        break;
                      default:
                        output = V6Zlib.inflateBytes(input, false);
                        break;
                      }
                      controller.get("enqueue").asCallable().call(
                          objValue(controller),
                          new V6Value[] {objValue(new V6Buffer(output))});
                      return UNDEF;
                    }));

    return new V6TransformStreamConstructor().construct(
        new V6Value[] {objValue(transformer)});
  }
}
