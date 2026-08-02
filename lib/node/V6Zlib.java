import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.util.zip.DataFormatException;
import java.util.zip.Deflater;
import java.util.zip.GZIPInputStream;
import java.util.zip.GZIPOutputStream;
import java.util.zip.Inflater;

public final class V6Zlib {
  private V6Zlib() {}

  private static V6Value fn(V6Callable c) {
    return new V6Value(V6Value.TAG_FUNC, 0, c);
  }

  private static V6Value objValue(V6Object o) {
    return new V6Value(V6Value.TAG_OBJ, 0, o);
  }

  private static final V6Value UNDEF = new V6Value(V6Value.TAG_UNDEF, 0, null);
  private static final V6Value NUL = new V6Value(V6Value.TAG_NULL, 0, null);

  private static byte[] bytesOf(V6Value v) {
    if (v.tag() == V6Value.TAG_OBJ && v.ref() instanceof V6Buffer)
      return ((V6Buffer)v.ref()).toBytes();
    return v.toString().getBytes(java.nio.charset.StandardCharsets.UTF_8);
  }

  private static byte[] deflateBytes(byte[] input, boolean raw) {
    Deflater def = new Deflater(Deflater.DEFAULT_COMPRESSION, raw);
    def.setInput(input);
    def.finish();
    ByteArrayOutputStream out = new ByteArrayOutputStream();
    byte[] buf = new byte[8192];
    while (!def.finished()) {
      int n = def.deflate(buf);
      out.write(buf, 0, n);
    }
    def.end();
    return out.toByteArray();
  }

  private static byte[] inflateBytes(byte[] input, boolean raw) {
    Inflater inf = new Inflater(raw);
    inf.setInput(input);
    ByteArrayOutputStream out = new ByteArrayOutputStream();
    byte[] buf = new byte[8192];
    try {
      while (!inf.finished()) {
        int n = inf.inflate(buf);
        if (n == 0 && (inf.needsInput() || inf.needsDictionary()))
          break;
        out.write(buf, 0, n);
      }
    } catch (DataFormatException e) {
      throw new V6Throw(new V6Value(V6Value.TAG_STR, 0, "zlib: " + e.getMessage()));
    } finally {
      inf.end();
    }
    return out.toByteArray();
  }

  private static byte[] gzipBytes(byte[] input) {
    ByteArrayOutputStream bos = new ByteArrayOutputStream();
    try (GZIPOutputStream gz = new GZIPOutputStream(bos)) {
      gz.write(input);
    } catch (IOException e) {
      throw new V6Throw(new V6Value(V6Value.TAG_STR, 0, "zlib gzip: " + e.getMessage()));
    }
    return bos.toByteArray();
  }

  private static byte[] gunzipBytes(byte[] input) {
    try (GZIPInputStream gis = new GZIPInputStream(new ByteArrayInputStream(input))) {
      return gis.readAllBytes();
    } catch (IOException e) {
      throw new V6Throw(new V6Value(V6Value.TAG_STR, 0, "zlib gunzip: " + e.getMessage()));
    }
  }

  private static void wireAsync(V6Object o, String asyncName, String syncName) {
    o.set(asyncName, fn((thisArg, args) -> {
            V6Callable cb = args[args.length - 1].asCallable();
            V6Value[] syncArgs = java.util.Arrays.copyOf(args, args.length - 1);
            try {
              V6Value result = o.get(syncName).asCallable().call(thisArg, syncArgs);
              V6MicrotaskQueue.enqueue(() -> cb.call(UNDEF, new V6Value[] {NUL, result}));
            } catch (V6Throw e) {
              V6MicrotaskQueue.enqueue(() -> cb.call(UNDEF, new V6Value[] {e.value, UNDEF}));
            }
            return UNDEF;
          }));
  }

  public static V6Object build() {
    V6Object o = new V6Object();

    o.set("gzipSync", fn((t, a) -> objValue(new V6Buffer(gzipBytes(bytesOf(V6Value.argAt(a, 0)))))));
    o.set("gunzipSync",
          fn((t, a) -> objValue(new V6Buffer(gunzipBytes(bytesOf(V6Value.argAt(a, 0)))))));
    o.set("deflateSync",
          fn((t, a)
                 -> objValue(new V6Buffer(deflateBytes(bytesOf(V6Value.argAt(a, 0)), false)))));
    o.set("inflateSync",
          fn((t, a)
                 -> objValue(new V6Buffer(inflateBytes(bytesOf(V6Value.argAt(a, 0)), false)))));
    o.set("deflateRawSync",
          fn((t, a)
                 -> objValue(new V6Buffer(deflateBytes(bytesOf(V6Value.argAt(a, 0)), true)))));
    o.set("inflateRawSync",
          fn((t, a)
                 -> objValue(new V6Buffer(inflateBytes(bytesOf(V6Value.argAt(a, 0)), true)))));

    wireAsync(o, "gzip", "gzipSync");
    wireAsync(o, "gunzip", "gunzipSync");
    wireAsync(o, "deflate", "deflateSync");
    wireAsync(o, "inflate", "inflateSync");
    wireAsync(o, "deflateRaw", "deflateRawSync");
    wireAsync(o, "inflateRaw", "inflateRawSync");

    return o;
  }
}
