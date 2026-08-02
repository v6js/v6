import java.nio.charset.StandardCharsets;

public final class V6V8 {
  private V6V8() {
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

  private static final V6Value UNDEF = new V6Value(V6Value.TAG_UNDEF, 0, null);

  public static V6Object build() {
    V6Object o = new V6Object();

    o.set("getHeapStatistics", fn((thisArg, args) -> {
            Runtime rt = Runtime.getRuntime();
            V6Object result = new V6Object();
            result.set("total_heap_size", num(rt.totalMemory()));
            result.set("used_heap_size",
                       num(rt.totalMemory() - rt.freeMemory()));
            result.set("heap_size_limit", num(rt.maxMemory()));
            result.set("total_available_size", num(rt.freeMemory()));
            result.set("malloced_memory", num(0));
            result.set("peak_malloced_memory", num(0));
            result.set("does_zap_garbage", num(0));
            result.set("number_of_native_contexts", num(1));
            result.set("number_of_detached_contexts", num(0));
            return objValue(result);
          }));

    o.set("getHeapSpaceStatistics", fn((thisArg, args) -> {
            Runtime rt = Runtime.getRuntime();
            V6Array result = new V6Array();
            V6Object space = new V6Object();
            space.set("space_name", str("heap"));
            space.set("space_size", num(rt.totalMemory()));
            space.set("space_used_size",
                      num(rt.totalMemory() - rt.freeMemory()));
            space.set("space_available_size", num(rt.freeMemory()));
            space.set("physical_space_size", num(rt.totalMemory()));
            result.push(objValue(space));
            return objValue(result);
          }));

    o.set("setFlagsFromString", fn((thisArg, args) -> UNDEF));

    o.set(
        "serialize", fn((thisArg, args) -> {
          String json =
              V6Json.stringify(V6Value.argAt(args, 0), UNDEF, UNDEF).toString();
          return objValue(new V6Buffer(json.getBytes(StandardCharsets.UTF_8)));
        }));

    o.set("deserialize", fn((thisArg, args) -> {
            V6Value v = V6Value.argAt(args, 0);
            byte[] bytes =
                v.tag() == V6Value.TAG_OBJ && v.ref() instanceof V6Buffer
                    ? ((V6Buffer)v.ref()).toBytes()
                    : new byte[0];
            String json = new String(bytes, StandardCharsets.UTF_8);
            return V6Json.parse(json);
          }));

    o.set("writeHeapSnapshot", fn((thisArg, args) -> {
            throw new V6Throw(str("v8.writeHeapSnapshot is not supported (no " +
                                  "JVM-native V8 heap snapshot format)"));
          }));

    return o;
  }
}
