import java.nio.ByteBuffer;
import java.nio.CharBuffer;
import java.nio.charset.Charset;
import java.nio.charset.CharsetDecoder;
import java.nio.charset.CodingErrorAction;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;

public final class V6StringDecoderObject extends V6Object {
  private final CharsetDecoder decoder;
  private byte[] pending = new byte[0];

  V6StringDecoderObject(String encoding) {
    Charset cs;
    switch (encoding == null ? "utf8" : encoding.toLowerCase()) {
    case "ascii":
    case "latin1":
    case "binary":
      cs = StandardCharsets.ISO_8859_1;
      break;
    case "utf16le":
    case "ucs2":
    case "ucs-2":
      cs = StandardCharsets.UTF_16LE;
      break;
    default:
      cs = StandardCharsets.UTF_8;
    }
    decoder = cs.newDecoder()
                  .onMalformedInput(CodingErrorAction.REPLACE)
                  .onUnmappableCharacter(CodingErrorAction.REPLACE);
  }

  private static byte[] concat(byte[] a, byte[] b) {
    if (a.length == 0)
      return b;
    if (b.length == 0)
      return a;
    byte[] out = new byte[a.length + b.length];
    System.arraycopy(a, 0, out, 0, a.length);
    System.arraycopy(b, 0, out, a.length, b.length);
    return out;
  }

  public String write(byte[] newBytes) {
    byte[] combined = concat(pending, newBytes);
    ByteBuffer bb = ByteBuffer.wrap(combined);
    CharBuffer cb = CharBuffer.allocate(combined.length * 2 + 16);
    decoder.decode(bb, cb, false);
    int consumed = bb.position();
    pending = Arrays.copyOfRange(combined, consumed, combined.length);
    cb.flip();
    return cb.toString();
  }

  public String end(byte[] newBytes) {
    byte[] combined = concat(pending, newBytes);
    pending = new byte[0];
    ByteBuffer bb = ByteBuffer.wrap(combined);
    CharBuffer cb = CharBuffer.allocate(combined.length * 2 + 16);
    decoder.decode(bb, cb, true);
    decoder.flush(cb);
    decoder.reset();
    cb.flip();
    return cb.toString();
  }
}
