import java.security.KeyFactory;
import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.MessageDigest;
import java.security.PrivateKey;
import java.security.PublicKey;
import java.security.SecureRandom;
import java.security.Signature;
import java.security.spec.PKCS8EncodedKeySpec;
import java.security.spec.X509EncodedKeySpec;
import javax.crypto.Cipher;
import javax.crypto.KeyGenerator;
import javax.crypto.Mac;
import javax.crypto.spec.GCMParameterSpec;
import javax.crypto.spec.IvParameterSpec;
import javax.crypto.spec.SecretKeySpec;

public final class V6WebCrypto {
  private V6WebCrypto() {
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
  private static final SecureRandom RANDOM = new SecureRandom();

  private static byte[] bytesOf(V6Value v) {
    if (v.tag() == V6Value.TAG_OBJ && v.ref() instanceof V6Buffer)
      return ((V6Buffer)v.ref()).toBytes();
    if (v.tag() == V6Value.TAG_OBJ && v.ref() instanceof V6ArrayBufferObject)
      return ((V6ArrayBufferObject)v.ref()).data;
    return new byte[0];
  }

  private static String algoName(V6Value algoVal) {
    if (algoVal.tag() == V6Value.TAG_STR)
      return algoVal.toString();
    if (algoVal.tag() == V6Value.TAG_OBJ && algoVal.ref() instanceof V6Object)
      return ((V6Object)algoVal.ref()).get("name").toString();
    return "";
  }

  private static V6Object algoAsObject(V6Value algoVal) {
    if (algoVal.tag() == V6Value.TAG_OBJ && algoVal.ref() instanceof V6Object)
      return (V6Object)algoVal.ref();
    V6Object o = new V6Object();
    o.set("name", str(algoName(algoVal)));
    return o;
  }

  private static String digestAlgo(String webName) {
    switch (webName.toUpperCase()) {
    case "SHA-1":
      return "SHA-1";
    case "SHA-256":
      return "SHA-256";
    case "SHA-384":
      return "SHA-384";
    case "SHA-512":
      return "SHA-512";
    default:
      throw new V6Throw(
          str("NotSupportedError: unsupported digest algorithm " + webName));
    }
  }

  public static V6Object build() {
    V6Object o = new V6Object();

    o.set("getRandomValues", fn((thisArg, args) -> {
            V6Value v = V6Value.argAt(args, 0);
            if (v.tag() == V6Value.TAG_OBJ && v.ref() instanceof V6Buffer) {
              V6Buffer buf = (V6Buffer)v.ref();
              int len = (int)buf.get("length").num();
              byte[] rnd = new byte[len];
              RANDOM.nextBytes(rnd);
              for (int i = 0; i < len; i++)
                buf.set(Integer.toString(i), num(rnd[i] & 0xFF));
            }
            return v;
          }));

    o.set("randomUUID",
          fn((thisArg, args) -> str(java.util.UUID.randomUUID().toString())));

    o.set("subtle", objValue(buildSubtle()));

    return o;
  }

  private static V6Object buildSubtle() {
    V6Object o = new V6Object();

    o.set("digest", fn((thisArg, args) -> {
            String name = digestAlgo(algoName(V6Value.argAt(args, 0)));
            byte[] data = bytesOf(V6Value.argAt(args, 1));
            try {
              MessageDigest md = MessageDigest.getInstance(name);
              byte[] result = md.digest(data);
              return objValue(
                  V6Promise.resolved(V6ArrayBufferConstructor.wrap(result)));
            } catch (Exception e) {
              return objValue(
                  V6Promise.rejected(str("OperationError: " + e.getMessage())));
            }
          }));

    o.set("generateKey", fn((thisArg, args) -> generateKey(args)));
    o.set("importKey", fn((thisArg, args) -> importKey(args)));
    o.set("exportKey", fn((thisArg, args) -> exportKey(args)));
    o.set("encrypt", fn((thisArg, args) -> cipherOp(args, true)));
    o.set("decrypt", fn((thisArg, args) -> cipherOp(args, false)));
    o.set("sign", fn((thisArg, args) -> signOp(args)));
    o.set("verify", fn((thisArg, args) -> verifyOp(args)));

    return o;
  }

  private static V6Value generateKey(V6Value[] args) {
    V6Value algoVal = V6Value.argAt(args, 0);
    String name = algoName(algoVal);
    V6Object algoObj = algoAsObject(algoVal);
    boolean extractable = V6Value.argAt(args, 1).truthy();
    V6Value usagesVal = V6Value.argAt(args, 2);

    try {
      switch (name.toUpperCase()) {
      case "AES-GCM":
      case "AES-CBC":
      case "AES-CTR": {
        int length = algoObj.props.containsKey("length")
                         ? (int)algoObj.get("length").toNumber()
                         : 256;
        KeyGenerator kg = KeyGenerator.getInstance("AES");
        kg.init(length, RANDOM);
        byte[] keyBytes = kg.generateKey().getEncoded();
        V6CryptoKeyObject key = V6CryptoKeyConstructor.newKey();
        key.keyMaterial = keyBytes;
        key.algorithmName = name;
        key.algorithmObj = algoObj;
        key.type = "secret";
        key.extractable = extractable;
        key.usages = usagesVal.tag() == V6Value.TAG_OBJ &&
                             usagesVal.ref() instanceof V6Array
                         ? (V6Array)usagesVal.ref()
                         : new V6Array();
        return objValue(V6Promise.resolved(objValue(key)));
      }
      case "HMAC": {
        V6Value hashVal = algoObj.get("hash");
        String hashName = digestAlgo(algoName(hashVal));
        int length = algoObj.props.containsKey("length")
                         ? (int)algoObj.get("length").toNumber()
                         : (hashName.equals("SHA-1") ? 512 : 1024);
        byte[] keyBytes = new byte[length / 8];
        RANDOM.nextBytes(keyBytes);
        V6CryptoKeyObject key = V6CryptoKeyConstructor.newKey();
        key.keyMaterial = keyBytes;
        key.algorithmName = name;
        key.algorithmObj = algoObj;
        key.type = "secret";
        key.extractable = extractable;
        return objValue(V6Promise.resolved(objValue(key)));
      }
      case "RSASSA-PKCS1-V1_5":
      case "RSA-OAEP":
      case "RSA-PSS": {
        int modulusLength = algoObj.get("modulusLength").isUndefined()
                                ? 2048
                                : (int)algoObj.get("modulusLength").toNumber();
        KeyPairGenerator kpg = KeyPairGenerator.getInstance("RSA");
        kpg.initialize(modulusLength);
        KeyPair kp = kpg.generateKeyPair();

        V6CryptoKeyObject pubKey = V6CryptoKeyConstructor.newKey();
        pubKey.keyMaterial = kp.getPublic();
        pubKey.algorithmName = name;
        pubKey.algorithmObj = algoObj;
        pubKey.type = "public";
        pubKey.extractable = true;

        V6CryptoKeyObject privKey = V6CryptoKeyConstructor.newKey();
        privKey.keyMaterial = kp.getPrivate();
        privKey.algorithmName = name;
        privKey.algorithmObj = algoObj;
        privKey.type = "private";
        privKey.extractable = extractable;

        V6Object pair = new V6Object();
        pair.set("publicKey", objValue(pubKey));
        pair.set("privateKey", objValue(privKey));
        return objValue(V6Promise.resolved(objValue(pair)));
      }
      default:
        return objValue(V6Promise.rejected(
            str("NotSupportedError: unsupported algorithm " + name)));
      }
    } catch (Exception e) {
      return objValue(
          V6Promise.rejected(str("OperationError: " + e.getMessage())));
    }
  }

  private static V6Value importKey(V6Value[] args) {
    String format = V6Value.argAt(args, 0).toString();
    byte[] keyData = bytesOf(V6Value.argAt(args, 1));
    V6Value algoVal = V6Value.argAt(args, 2);
    String name = algoName(algoVal);
    V6Object algoObj = algoAsObject(algoVal);
    boolean extractable = V6Value.argAt(args, 3).truthy();

    try {
      V6CryptoKeyObject key = V6CryptoKeyConstructor.newKey();
      key.algorithmName = name;
      key.algorithmObj = algoObj;
      key.extractable = extractable;

      switch (format) {
      case "raw":
        key.keyMaterial = keyData;
        key.type = "secret";
        break;
      case "spki": {
        KeyFactory kf = KeyFactory.getInstance("RSA");
        key.keyMaterial = kf.generatePublic(new X509EncodedKeySpec(keyData));
        key.type = "public";
        break;
      }
      case "pkcs8": {
        KeyFactory kf = KeyFactory.getInstance("RSA");
        key.keyMaterial = kf.generatePrivate(new PKCS8EncodedKeySpec(keyData));
        key.type = "private";
        break;
      }
      default:
        return objValue(V6Promise.rejected(
            str("NotSupportedError: unsupported key format " + format)));
      }
      return objValue(V6Promise.resolved(objValue(key)));
    } catch (Exception e) {
      return objValue(
          V6Promise.rejected(str("OperationError: " + e.getMessage())));
    }
  }

  private static V6Value exportKey(V6Value[] args) {
    String format = V6Value.argAt(args, 0).toString();
    V6Value keyVal = V6Value.argAt(args, 1);
    if (keyVal.tag() != V6Value.TAG_OBJ ||
        !(keyVal.ref() instanceof V6CryptoKeyObject))
      return objValue(V6Promise.rejected(str("TypeError: not a CryptoKey")));
    V6CryptoKeyObject key = (V6CryptoKeyObject)keyVal.ref();
    try {
      byte[] out;
      switch (format) {
      case "raw":
        out = (byte[])key.keyMaterial;
        break;
      case "spki":
        out = ((PublicKey)key.keyMaterial).getEncoded();
        break;
      case "pkcs8":
        out = ((PrivateKey)key.keyMaterial).getEncoded();
        break;
      default:
        return objValue(V6Promise.rejected(
            str("NotSupportedError: unsupported key format " + format)));
      }
      return objValue(V6Promise.resolved(V6ArrayBufferConstructor.wrap(out)));
    } catch (Exception e) {
      return objValue(
          V6Promise.rejected(str("OperationError: " + e.getMessage())));
    }
  }

  private static V6Value cipherOp(V6Value[] args, boolean encrypt) {
    V6Value algoVal = V6Value.argAt(args, 0);
    String name = algoName(algoVal).toUpperCase();
    V6Object algoObj = algoAsObject(algoVal);
    V6Value keyVal = V6Value.argAt(args, 1);
    byte[] data = bytesOf(V6Value.argAt(args, 2));

    if (keyVal.tag() != V6Value.TAG_OBJ ||
        !(keyVal.ref() instanceof V6CryptoKeyObject))
      return objValue(V6Promise.rejected(str("TypeError: not a CryptoKey")));
    V6CryptoKeyObject key = (V6CryptoKeyObject)keyVal.ref();

    try {
      byte[] iv = bytesOf(algoObj.get("iv"));
      String transformation;
      switch (name) {
      case "AES-GCM":
        transformation = "AES/GCM/NoPadding";
        break;
      case "AES-CBC":
        transformation = "AES/CBC/PKCS5Padding";
        break;
      case "AES-CTR":
        transformation = "AES/CTR/NoPadding";
        break;
      default:
        return objValue(V6Promise.rejected(
            str("NotSupportedError: unsupported cipher algorithm " + name)));
      }
      Cipher cipher = Cipher.getInstance(transformation);
      SecretKeySpec keySpec = new SecretKeySpec((byte[])key.keyMaterial, "AES");
      int mode = encrypt ? Cipher.ENCRYPT_MODE : Cipher.DECRYPT_MODE;
      if (name.equals("AES-GCM")) {
        int tagLength = algoObj.get("tagLength").isUndefined()
                            ? 128
                            : (int)algoObj.get("tagLength").toNumber();
        cipher.init(mode, keySpec, new GCMParameterSpec(tagLength, iv));
      } else {
        cipher.init(mode, keySpec, new IvParameterSpec(iv));
      }
      byte[] result = cipher.doFinal(data);
      return objValue(
          V6Promise.resolved(V6ArrayBufferConstructor.wrap(result)));
    } catch (Exception e) {
      return objValue(
          V6Promise.rejected(str("OperationError: " + e.getMessage())));
    }
  }

  private static String hmacAlgo(String digestName) {
    switch (digestName) {
    case "SHA-1":
      return "HmacSHA1";
    case "SHA-256":
      return "HmacSHA256";
    case "SHA-384":
      return "HmacSHA384";
    case "SHA-512":
      return "HmacSHA512";
    default:
      throw new V6Throw(
          str("NotSupportedError: unsupported hash " + digestName));
    }
  }

  private static String rsaSigAlgo(String digestName) {
    switch (digestName) {
    case "SHA-1":
      return "SHA1withRSA";
    case "SHA-256":
      return "SHA256withRSA";
    case "SHA-384":
      return "SHA384withRSA";
    case "SHA-512":
      return "SHA512withRSA";
    default:
      throw new V6Throw(
          str("NotSupportedError: unsupported hash " + digestName));
    }
  }

  private static V6Value signOp(V6Value[] args) {
    V6Value algoVal = V6Value.argAt(args, 0);
    String name = algoName(algoVal).toUpperCase();
    V6Value keyVal = V6Value.argAt(args, 1);
    byte[] data = bytesOf(V6Value.argAt(args, 2));

    if (keyVal.tag() != V6Value.TAG_OBJ ||
        !(keyVal.ref() instanceof V6CryptoKeyObject))
      return objValue(V6Promise.rejected(str("TypeError: not a CryptoKey")));
    V6CryptoKeyObject key = (V6CryptoKeyObject)keyVal.ref();

    try {
      if (name.equals("HMAC")) {
        String hashName = digestAlgo(algoName(key.algorithmObj.get("hash")));
        String alg = hmacAlgo(hashName);
        Mac mac = Mac.getInstance(alg);
        mac.init(new SecretKeySpec((byte[])key.keyMaterial, alg));
        byte[] result = mac.doFinal(data);
        return objValue(
            V6Promise.resolved(V6ArrayBufferConstructor.wrap(result)));
      } else if (name.equals("RSASSA-PKCS1-V1_5")) {
        String hashName = digestAlgo(algoName(key.algorithmObj.get("hash")));
        Signature sig = Signature.getInstance(rsaSigAlgo(hashName));
        sig.initSign((PrivateKey)key.keyMaterial);
        sig.update(data);
        byte[] result = sig.sign();
        return objValue(
            V6Promise.resolved(V6ArrayBufferConstructor.wrap(result)));
      }
      return objValue(V6Promise.rejected(
          str("NotSupportedError: unsupported sign algorithm " + name)));
    } catch (Exception e) {
      return objValue(
          V6Promise.rejected(str("OperationError: " + e.getMessage())));
    }
  }

  private static V6Value verifyOp(V6Value[] args) {
    V6Value algoVal = V6Value.argAt(args, 0);
    String name = algoName(algoVal).toUpperCase();
    V6Value keyVal = V6Value.argAt(args, 1);
    byte[] signature = bytesOf(V6Value.argAt(args, 2));
    byte[] data = bytesOf(V6Value.argAt(args, 3));

    if (keyVal.tag() != V6Value.TAG_OBJ ||
        !(keyVal.ref() instanceof V6CryptoKeyObject))
      return objValue(V6Promise.rejected(str("TypeError: not a CryptoKey")));
    V6CryptoKeyObject key = (V6CryptoKeyObject)keyVal.ref();

    try {
      if (name.equals("HMAC")) {
        String hashName = digestAlgo(algoName(key.algorithmObj.get("hash")));
        String alg = hmacAlgo(hashName);
        Mac mac = Mac.getInstance(alg);
        mac.init(new SecretKeySpec((byte[])key.keyMaterial, alg));
        byte[] expected = mac.doFinal(data);
        return objValue(V6Promise.resolved(new V6Value(
            V6Value.TAG_BOOL,
            MessageDigest.isEqual(expected, signature) ? 1 : 0, null)));
      } else if (name.equals("RSASSA-PKCS1-V1_5")) {
        String hashName = digestAlgo(algoName(key.algorithmObj.get("hash")));
        Signature sig = Signature.getInstance(rsaSigAlgo(hashName));
        sig.initVerify((PublicKey)key.keyMaterial);
        sig.update(data);
        boolean ok = sig.verify(signature);
        return objValue(V6Promise.resolved(
            new V6Value(V6Value.TAG_BOOL, ok ? 1 : 0, null)));
      }
      return objValue(V6Promise.rejected(
          str("NotSupportedError: unsupported verify algorithm " + name)));
    } catch (Exception e) {
      return objValue(
          V6Promise.rejected(str("OperationError: " + e.getMessage())));
    }
  }
}
