import java.io.ByteArrayOutputStream;
import java.nio.charset.StandardCharsets;
import java.security.KeyFactory;
import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.security.PrivateKey;
import java.security.PublicKey;
import java.security.SecureRandom;
import java.security.Signature;
import java.util.Arrays;
import java.util.Base64;
import javax.crypto.Cipher;
import javax.crypto.Mac;
import javax.crypto.SecretKeyFactory;
import javax.crypto.spec.GCMParameterSpec;
import javax.crypto.spec.IvParameterSpec;
import javax.crypto.spec.PBEKeySpec;
import javax.crypto.spec.SecretKeySpec;

public final class V6Crypto {
  private V6Crypto() {
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
  private static final V6Value NUL = new V6Value(V6Value.TAG_NULL, 0, null);
  private static final SecureRandom RANDOM = new SecureRandom();

  private static byte[] bytesOf(V6Value data, String encoding) {
    if (data.tag() == V6Value.TAG_OBJ && data.ref() instanceof V6Buffer)
      return ((V6Buffer)data.ref()).toBytes();
    return V6BufferConstructor.decodeString(
        data.toString(), encoding != null ? encoding : "utf8");
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
    String enc =
        a.length > 0 && a[0].tag() == V6Value.TAG_STR ? a[0].toString() : null;
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
            String enc = a.length > 1 && a[1].tag() == V6Value.TAG_STR
                             ? a[1].toString()
                             : null;
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
      mac.init(new SecretKeySpec(keyBytes.length == 0 ? new byte[1] : keyBytes,
                                 alg));
    } catch (Exception e) {
      throw new V6Throw(str("crypto: unsupported hmac algorithm " + algoName));
    }
    V6Object o = new V6Object();
    o.set("update", fn((t, a) -> {
            String enc = a.length > 1 && a[1].tag() == V6Value.TAG_STR
                             ? a[1].toString()
                             : null;
            mac.update(bytesOf(V6Value.argAt(a, 0), enc));
            return t;
          }));
    o.set("digest", fn((t, a) -> digestResult(mac.doFinal(), a)));
    return o;
  }

  private static V6Value bool(boolean b) {
    return new V6Value(V6Value.TAG_BOOL, b ? 1 : 0, null);
  }

  private static void deferCallback(V6Callable cb, V6Value err,
                                    V6Value result) {
    V6MicrotaskQueue.enqueue(() -> cb.call(UNDEF, new V6Value[] {err, result}));
  }

  private static byte[] pemBytesOf(V6Value v) {
    if (v.tag() == V6Value.TAG_OBJ && v.ref() instanceof V6Object &&
        !(v.ref() instanceof V6Buffer)) {
      V6Value keyProp = ((V6Object)v.ref()).get("key");
      if (!keyProp.isUndefined())
        return pemBytesOf(keyProp);
    }
    if (v.tag() == V6Value.TAG_OBJ && v.ref() instanceof V6Buffer)
      return ((V6Buffer)v.ref()).toBytes();
    return v.toString().getBytes(StandardCharsets.UTF_8);
  }

  private static String cipherKeyAlgo(String name) {
    String n = name.toLowerCase();
    if (n.startsWith("aes-"))
      return "AES";
    if (n.startsWith("des-ede3"))
      return "DESede";
    if (n.startsWith("des-"))
      return "DES";
    throw new V6Throw(str("crypto: unsupported cipher algorithm " + name));
  }

  private static String cipherTransformation(String name) {
    String n = name.toLowerCase();
    if (n.startsWith("aes-")) {
      String[] parts = n.split("-");
      String mode = parts.length > 2 ? parts[2] : "cbc";
      switch (mode) {
      case "gcm":
        return "AES/GCM/NoPadding";
      case "ecb":
        return "AES/ECB/PKCS5Padding";
      case "ctr":
        return "AES/CTR/NoPadding";
      case "cfb":
        return "AES/CFB/NoPadding";
      case "ofb":
        return "AES/OFB/NoPadding";
      default:
        return "AES/CBC/PKCS5Padding";
      }
    }
    if (n.equals("des-ede3-cbc") || n.equals("des-ede3"))
      return "DESede/CBC/PKCS5Padding";
    if (n.equals("des-ede3-ecb"))
      return "DESede/ECB/PKCS5Padding";
    if (n.equals("des-cbc") || n.equals("des"))
      return "DES/CBC/PKCS5Padding";
    if (n.equals("des-ecb"))
      return "DES/ECB/PKCS5Padding";
    throw new V6Throw(str("crypto: unsupported cipher algorithm " + name));
  }

  private static V6Object buildCipher(boolean encrypt, String algorithm,
                                      byte[] key, byte[] iv) {
    String transformation = cipherTransformation(algorithm);
    String keyAlgo = cipherKeyAlgo(algorithm);
    boolean gcm = transformation.contains("/GCM/");
    boolean noIv = transformation.contains("/ECB/");
    Cipher cipher;
    try {
      cipher = Cipher.getInstance(transformation);
      SecretKeySpec keySpec = new SecretKeySpec(key, keyAlgo);
      int mode = encrypt ? Cipher.ENCRYPT_MODE : Cipher.DECRYPT_MODE;
      if (gcm)
        cipher.init(mode, keySpec, new GCMParameterSpec(128, iv));
      else if (noIv)
        cipher.init(mode, keySpec);
      else
        cipher.init(mode, keySpec, new IvParameterSpec(iv));
    } catch (Exception e) {
      throw new V6Throw(str("crypto: " + e.getMessage()));
    }

    ByteArrayOutputStream gcmBuffer = gcm ? new ByteArrayOutputStream() : null;
    byte[][] authTag = new byte[1][];
    V6Object o = new V6Object();

    o.set("update", fn((t, a) -> {
            String inEnc = a.length > 1 && a[1].tag() == V6Value.TAG_STR
                               ? a[1].toString()
                               : null;
            byte[] data = bytesOf(V6Value.argAt(a, 0), inEnc);
            byte[] out;
            if (gcm) {
              gcmBuffer.write(data, 0, data.length);
              out = new byte[0];
            } else {
              out = cipher.update(data);
            }
            V6Value[] encArgs =
                a.length > 2 ? new V6Value[] {a[2]} : new V6Value[0];
            return digestResult(out, encArgs);
          }));

    o.set("final", fn((t, a) -> {
            byte[] out;
            try {
              if (gcm) {
                if (encrypt) {
                  byte[] full = cipher.doFinal(gcmBuffer.toByteArray());
                  int tagLen = 16;
                  out = Arrays.copyOfRange(full, 0, full.length - tagLen);
                  authTag[0] = Arrays.copyOfRange(full, full.length - tagLen,
                                                  full.length);
                } else {
                  if (authTag[0] == null)
                    throw new V6Throw(str("crypto: authTag must be set via "
                                          + "setAuthTag before final() "
                                          + "for GCM decryption"));
                  byte[] ct = gcmBuffer.toByteArray();
                  byte[] combined = new byte[ct.length + authTag[0].length];
                  System.arraycopy(ct, 0, combined, 0, ct.length);
                  System.arraycopy(authTag[0], 0, combined, ct.length,
                                   authTag[0].length);
                  out = cipher.doFinal(combined);
                }
              } else {
                out = cipher.doFinal();
              }
            } catch (V6Throw e) {
              throw e;
            } catch (Exception e) {
              throw new V6Throw(str("crypto: " + e.getMessage()));
            }
            return digestResult(out, a);
          }));

    if (gcm) {
      o.set("setAAD", fn((t, a) -> {
              cipher.updateAAD(bytesOf(V6Value.argAt(a, 0), null));
              return t;
            }));
      if (encrypt)
        o.set("getAuthTag", fn((t, a) -> objValue(new V6Buffer(authTag[0]))));
      else
        o.set("setAuthTag", fn((t, a) -> {
                authTag[0] = bytesOf(V6Value.argAt(a, 0), null);
                return t;
              }));
    }
    return o;
  }

  private static String javaDigestName(String d) {
    switch (d.toLowerCase()) {
    case "sha1":
      return "SHA1";
    case "sha224":
      return "SHA224";
    case "sha256":
      return "SHA256";
    case "sha384":
      return "SHA384";
    case "sha512":
      return "SHA512";
    case "md5":
      return "MD5";
    default:
      return d.toUpperCase();
    }
  }

  private static String signatureAlgo(String name) {
    String lower = name.toLowerCase();
    if (lower.startsWith("rsa-"))
      return javaDigestName(lower.substring(4)) + "withRSA";
    if (lower.startsWith("ecdsa-with-"))
      return javaDigestName(lower.substring(11)) + "withECDSA";
    if (lower.startsWith("dsa-"))
      return javaDigestName(lower.substring(4)) + "withDSA";
    return javaDigestName(lower) + "withRSA";
  }

  private static V6Object buildSign(String algorithm) {
    String javaAlgo = signatureAlgo(algorithm);
    ByteArrayOutputStream buf = new ByteArrayOutputStream();
    V6Object o = new V6Object();
    o.set("update", fn((t, a) -> {
            String enc = a.length > 1 && a[1].tag() == V6Value.TAG_STR
                             ? a[1].toString()
                             : null;
            byte[] d = bytesOf(V6Value.argAt(a, 0), enc);
            buf.write(d, 0, d.length);
            return t;
          }));
    o.set("sign", fn((t, a) -> {
            try {
              PrivateKey pk =
                  V6TlsUtil.parsePrivateKey(pemBytesOf(V6Value.argAt(a, 0)));
              Signature sig = Signature.getInstance(javaAlgo);
              sig.initSign(pk);
              sig.update(buf.toByteArray());
              byte[] result = sig.sign();
              V6Value[] encArgs =
                  a.length > 1 ? new V6Value[] {a[1]} : new V6Value[0];
              return digestResult(result, encArgs);
            } catch (Exception e) {
              throw new V6Throw(str("crypto: " + e.getMessage()));
            }
          }));
    return o;
  }

  private static V6Object buildVerify(String algorithm) {
    String javaAlgo = signatureAlgo(algorithm);
    ByteArrayOutputStream buf = new ByteArrayOutputStream();
    V6Object o = new V6Object();
    o.set("update", fn((t, a) -> {
            String enc = a.length > 1 && a[1].tag() == V6Value.TAG_STR
                             ? a[1].toString()
                             : null;
            byte[] d = bytesOf(V6Value.argAt(a, 0), enc);
            buf.write(d, 0, d.length);
            return t;
          }));
    o.set("verify", fn((t, a) -> {
            try {
              PublicKey pub =
                  V6TlsUtil.parsePublicKey(pemBytesOf(V6Value.argAt(a, 0)));
              Signature sig = Signature.getInstance(javaAlgo);
              sig.initVerify(pub);
              sig.update(buf.toByteArray());
              String sigEnc = a.length > 2 && a[2].tag() == V6Value.TAG_STR
                                  ? a[2].toString()
                                  : null;
              byte[] sigBytes = bytesOf(V6Value.argAt(a, 1), sigEnc);
              return bool(sig.verify(sigBytes));
            } catch (Exception e) {
              throw new V6Throw(str("crypto: " + e.getMessage()));
            }
          }));
    return o;
  }

  private static byte[] pbkdf2(byte[] password, byte[] salt, int iterations,
                               int keylen, String digest) {
    try {
      String algo;
      switch (digest.toLowerCase()) {
      case "sha1":
        algo = "PBKDF2WithHmacSHA1";
        break;
      case "sha224":
        algo = "PBKDF2WithHmacSHA224";
        break;
      case "sha256":
        algo = "PBKDF2WithHmacSHA256";
        break;
      case "sha384":
        algo = "PBKDF2WithHmacSHA384";
        break;
      case "sha512":
        algo = "PBKDF2WithHmacSHA512";
        break;
      default:
        algo = "PBKDF2WithHmacSHA1";
        break;
      }
      SecretKeyFactory skf = SecretKeyFactory.getInstance(algo);
      char[] pwChars =
          new String(password, StandardCharsets.ISO_8859_1).toCharArray();
      PBEKeySpec spec = new PBEKeySpec(pwChars, salt, iterations, keylen * 8);
      return skf.generateSecret(spec).getEncoded();
    } catch (Exception e) {
      throw new V6Throw(str("crypto: pbkdf2 failed: " + e.getMessage()));
    }
  }

  private static String pemEncode(String label, byte[] der) {
    StringBuilder sb = new StringBuilder();
    sb.append("-----BEGIN ").append(label).append("-----\n");
    String b64 = Base64.getEncoder().encodeToString(der);
    for (int i = 0; i < b64.length(); i += 64)
      sb.append(b64, i, Math.min(i + 64, b64.length())).append("\n");
    sb.append("-----END ").append(label).append("-----\n");
    return sb.toString();
  }

  private static V6Object generateKeyPairSyncImpl(V6Value[] args) {
    String type = V6Value.argAt(args, 0).toString();
    if (!type.equalsIgnoreCase("rsa"))
      throw new V6Throw(str(
          "crypto.generateKeyPairSync: only the 'rsa' key type is supported"));

    int modulusLength = 2048;
    String pubFormat = "pem";
    String privFormat = "pem";
    V6Value optsVal = V6Value.argAt(args, 1);
    if (optsVal.tag() == V6Value.TAG_OBJ && optsVal.ref() instanceof V6Object) {
      V6Object opts = (V6Object)optsVal.ref();
      V6Value ml = opts.get("modulusLength");
      if (!ml.isUndefined())
        modulusLength = (int)ml.toNumber();
      V6Value pubEnc = opts.get("publicKeyEncoding");
      if (pubEnc.tag() == V6Value.TAG_OBJ && pubEnc.ref() instanceof V6Object) {
        V6Value f = ((V6Object)pubEnc.ref()).get("format");
        if (!f.isUndefined())
          pubFormat = f.toString();
      }
      V6Value privEnc = opts.get("privateKeyEncoding");
      if (privEnc.tag() == V6Value.TAG_OBJ && privEnc.ref() instanceof
                                                  V6Object) {
        V6Value f = ((V6Object)privEnc.ref()).get("format");
        if (!f.isUndefined())
          privFormat = f.toString();
      }
    }

    try {
      KeyPairGenerator kpg = KeyPairGenerator.getInstance("RSA");
      kpg.initialize(modulusLength);
      KeyPair kp = kpg.generateKeyPair();
      byte[] pubDer = kp.getPublic().getEncoded();
      byte[] privDer = kp.getPrivate().getEncoded();
      V6Value pubResult = pubFormat.equals("der")
                              ? objValue(new V6Buffer(pubDer))
                              : str(pemEncode("PUBLIC KEY", pubDer));
      V6Value privResult = privFormat.equals("der")
                               ? objValue(new V6Buffer(privDer))
                               : str(pemEncode("PRIVATE KEY", privDer));
      V6Object result = new V6Object();
      result.set("publicKey", pubResult);
      result.set("privateKey", privResult);
      return result;
    } catch (Exception e) {
      throw new V6Throw(str("crypto.generateKeyPairSync: " + e.getMessage()));
    }
  }

  public static V6Object build() {
    V6Object o = new V6Object();

    o.set("createHash",
          fn((thisArg,
              args) -> objValue(buildHash(V6Value.argAt(args, 0).toString()))));

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
            return new V6Value(V6Value.TAG_NUM, lo + RANDOM.nextInt(range),
                               null);
          }));

    o.set("createCipheriv", fn((thisArg, args) -> {
            String algo = V6Value.argAt(args, 0).toString();
            byte[] key = bytesOf(V6Value.argAt(args, 1), null);
            V6Value ivVal = V6Value.argAt(args, 2);
            byte[] iv = ivVal.tag() == V6Value.TAG_NULL || ivVal.isUndefined()
                            ? new byte[0]
                            : bytesOf(ivVal, null);
            return objValue(buildCipher(true, algo, key, iv));
          }));

    o.set("createDecipheriv", fn((thisArg, args) -> {
            String algo = V6Value.argAt(args, 0).toString();
            byte[] key = bytesOf(V6Value.argAt(args, 1), null);
            V6Value ivVal = V6Value.argAt(args, 2);
            byte[] iv = ivVal.tag() == V6Value.TAG_NULL || ivVal.isUndefined()
                            ? new byte[0]
                            : bytesOf(ivVal, null);
            return objValue(buildCipher(false, algo, key, iv));
          }));

    o.set("createSign",
          fn((thisArg,
              args) -> objValue(buildSign(V6Value.argAt(args, 0).toString()))));

    o.set(
        "createVerify",
        fn((thisArg,
            args) -> objValue(buildVerify(V6Value.argAt(args, 0).toString()))));

    o.set("timingSafeEqual", fn((thisArg, args) -> {
            byte[] x = bytesOf(V6Value.argAt(args, 0), null);
            byte[] y = bytesOf(V6Value.argAt(args, 1), null);
            if (x.length != y.length)
              throw new V6Throw(str("crypto.timingSafeEqual: Input buffers "
                                    + "must have the same byte length"));
            return bool(MessageDigest.isEqual(x, y));
          }));

    o.set("pbkdf2Sync", fn((thisArg, args) -> {
            byte[] pw = bytesOf(V6Value.argAt(args, 0), null);
            byte[] salt = bytesOf(V6Value.argAt(args, 1), null);
            int iterations = (int)V6Value.argAt(args, 2).toNumber();
            int keylen = (int)V6Value.argAt(args, 3).toNumber();
            String digest = args.length > 4 && args[4].tag() == V6Value.TAG_STR
                                ? args[4].toString()
                                : "sha1";
            return objValue(
                new V6Buffer(pbkdf2(pw, salt, iterations, keylen, digest)));
          }));

    o.set("pbkdf2", fn((thisArg, args) -> {
            byte[] pw = bytesOf(V6Value.argAt(args, 0), null);
            byte[] salt = bytesOf(V6Value.argAt(args, 1), null);
            int iterations = (int)V6Value.argAt(args, 2).toNumber();
            int keylen = (int)V6Value.argAt(args, 3).toNumber();
            String digest = args.length > 5 && args[4].tag() == V6Value.TAG_STR
                                ? args[4].toString()
                                : "sha1";
            V6Callable cb = args[args.length - 1].asCallable();
            V6MicrotaskQueue.enqueue(() -> {
              try {
                byte[] result = pbkdf2(pw, salt, iterations, keylen, digest);
                deferCallback(cb, NUL, objValue(new V6Buffer(result)));
              } catch (V6Throw e) {
                deferCallback(cb, e.value, UNDEF);
              }
            });
            return UNDEF;
          }));

    o.set("generateKeyPairSync",
          fn((thisArg, args) -> objValue(generateKeyPairSyncImpl(args))));

    o.set("generateKeyPair", fn((thisArg, args) -> {
            V6Callable cb = args[args.length - 1].asCallable();
            V6Value[] syncArgs = Arrays.copyOf(args, args.length - 1);
            V6MicrotaskQueue.enqueue(() -> {
              try {
                V6Object result = generateKeyPairSyncImpl(syncArgs);
                cb.call(UNDEF, new V6Value[] {NUL, result.get("publicKey"),
                                              result.get("privateKey")});
              } catch (V6Throw e) {
                cb.call(UNDEF, new V6Value[] {e.value, UNDEF, UNDEF});
              }
            });
            return UNDEF;
          }));

    return o;
  }
}
