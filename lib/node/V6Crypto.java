import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.security.SecureRandom;
import javax.crypto.Mac;
import javax.crypto.spec.SecretKeySpec;

public final class V6Crypto {
  private V6Crypto() {}

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
  private static final V6Value NUL = new V6Value(V6Value.TAG_NULL, 0, null);
  private static final SecureRandom RANDOM = new SecureRandom();

  private static byte[] bytesOf(V6Value data, String encoding) {
    if (data.tag() == V6Value.TAG_OBJ && data.ref() instanceof V6Buffer)
      return ((V6Buffer)data.ref()).toBytes();
    return V6BufferConstructor.decodeString(data.toString(),
                                            encoding != null ? encoding : "utf8");
  }

  private static String digestAlgo(String name) {
    switch (name.toLowerCase()) {
    case "md5":
      return "MD5";
    case "sha1":
      return "SHA-1";
    case "sha224":
      return "SHA-224";
    case "sha256":
      return "SHA-256";
    case "sha384":
      return "SHA-384";
    case "sha512":
      return "SHA-512";
    default:
      return name.toUpperCase();
    }
  }

  private static String hmacAlgo(String name) {
    switch (name.toLowerCase()) {
    case "md5":
      return "HmacMD5";
    case "sha1":
      return "HmacSHA1";
    case "sha224":
      return "HmacSHA224";
    case "sha256":
      return "HmacSHA256";
    case "sha384":
      return "HmacSHA384";
    case "sha512":
      return "HmacSHA512";
    default:
      return "Hmac" + name.toUpperCase();
    }
  }

  private static V6Value digestResult(byte[] result, V6Value[] a) {
    String enc = a.length > 0 && a[0].tag() == V6Value.TAG_STR ? a[0].toString() : null;
    if (enc != null)
      return str(V6BufferConstructor.encodeBytes(result, enc));
    return objValue(new V6Buffer(result));
  }

  private static V6Object buildHash(String algoName) {
    MessageDigest md;
    try {
      md = MessageDigest.getInstance(digestAlgo(algoName));
    } catch (NoSuchAlgorithmException e) {
      throw new V6Throw(str("crypto: unsupported hash algorithm " + algoName));
    }
    V6Object o = new V6Object();
    o.set("update", fn((t, a) -> {
            String enc = a.length > 1 && a[1].tag() == V6Value.TAG_STR ? a[1].toString() : null;
            md.update(bytesOf(V6Value.argAt(a, 0), enc));
            return t;
          }));
    o.set("digest", fn((t, a) -> digestResult(md.digest(), a)));
    return o;
  }

  private static V6Object buildHmac(String algoName, byte[] keyBytes) {
    Mac mac;
    try {
      String alg = hmacAlgo(algoName);
      mac = Mac.getInstance(alg);
      mac.init(new SecretKeySpec(keyBytes.length == 0 ? new byte[1] : keyBytes, alg));
    } catch (Exception e) {
      throw new V6Throw(str("crypto: unsupported hmac algorithm " + algoName));
    }
    V6Object o = new V6Object();
    o.set("update", fn((t, a) -> {
            String enc = a.length > 1 && a[1].tag() == V6Value.TAG_STR ? a[1].toString() : null;
            mac.update(bytesOf(V6Value.argAt(a, 0), enc));
            return t;
          }));
    o.set("digest", fn((t, a) -> digestResult(mac.doFinal(), a)));
    return o;
  }

  public static V6Object build() {
    V6Object o = new V6Object();

    o.set("createHash",
          fn((thisArg, args) -> objValue(buildHash(V6Value.argAt(args, 0).toString()))));

    o.set("createHmac", fn((thisArg, args) -> {
            String algo = V6Value.argAt(args, 0).toString();
            byte[] key = bytesOf(V6Value.argAt(args, 1), null);
            return objValue(buildHmac(algo, key));
          }));

    o.set("randomBytes", fn((thisArg, args) -> {
            int size = (int)V6Value.argAt(args, 0).toNumber();
            byte[] bytes = new byte[Math.max(0, size)];
            RANDOM.nextBytes(bytes);
            V6Buffer buf = new V6Buffer(bytes);
            if (args.length > 1 && args[1].tag() == V6Value.TAG_FUNC) {
              V6Callable cb = args[1].asCallable();
              V6MicrotaskQueue.enqueue(
                  () -> cb.call(UNDEF, new V6Value[] {NUL, objValue(buf)}));
              return UNDEF;
            }
            return objValue(buf);
          }));

    o.set("randomUUID",
          fn((thisArg, args) -> str(java.util.UUID.randomUUID().toString())));

    o.set("randomInt", fn((thisArg, args) -> {
            int lo, hi;
            if (args.length >= 2) {
              lo = (int)args[0].toNumber();
              hi = (int)args[1].toNumber();
            } else {
              lo = 0;
              hi = (int)V6Value.argAt(args, 0).toNumber();
            }
            int range = Math.max(1, hi - lo);
            return new V6Value(V6Value.TAG_NUM, lo + RANDOM.nextInt(range), null);
          }));

    return o;
  }
}
