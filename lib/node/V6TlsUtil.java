import java.io.ByteArrayInputStream;
import java.nio.charset.StandardCharsets;
import java.security.KeyFactory;
import java.security.KeyStore;
import java.security.PrivateKey;
import java.security.cert.Certificate;
import java.security.cert.CertificateFactory;
import java.security.spec.PKCS8EncodedKeySpec;
import java.util.ArrayList;
import java.util.Base64;
import java.util.List;
import javax.net.ssl.KeyManagerFactory;
import javax.net.ssl.SSLContext;

public final class V6TlsUtil {
  private V6TlsUtil() {
  }

  private static byte[] wrapTag(int tag, byte[] body) {
    byte[] lenBytes = encodeLength(body.length);
    byte[] out = new byte[1 + lenBytes.length + body.length];
    out[0] = (byte)tag;
    System.arraycopy(lenBytes, 0, out, 1, lenBytes.length);
    System.arraycopy(body, 0, out, 1 + lenBytes.length, body.length);
    return out;
  }

  private static byte[] encodeLength(int len) {
    if (len < 128)
      return new byte[] {(byte)len};
    List<Byte> bytes = new ArrayList<>();
    int n = len;
    while (n > 0) {
      bytes.add(0, (byte)(n & 0xFF));
      n >>= 8;
    }
    byte[] out = new byte[bytes.size() + 1];
    out[0] = (byte)(0x80 | bytes.size());
    for (int i = 0; i < bytes.size(); i++)
      out[i + 1] = bytes.get(i);
    return out;
  }

  private static byte[] concat(byte[] a, byte[] b) {
    byte[] out = new byte[a.length + b.length];
    System.arraycopy(a, 0, out, 0, a.length);
    System.arraycopy(b, 0, out, a.length, b.length);
    return out;
  }

  private static byte[] convertPkcs1RsaToPkcs8(byte[] pkcs1) {
    byte[] rsaOid = {0x06,       0x09, 0x2A, (byte)0x86, 0x48, (byte)0x86,
                     (byte)0xF7, 0x0D, 0x01, 0x01,       0x01};
    byte[] algoIdSeq = wrapTag(0x30, concat(rsaOid, new byte[] {0x05, 0x00}));
    byte[] version = {0x02, 0x01, 0x00};
    byte[] octetString = wrapTag(0x04, pkcs1);
    byte[] body = concat(concat(version, algoIdSeq), octetString);
    return wrapTag(0x30, body);
  }

  static PrivateKey parsePrivateKey(byte[] pem) throws Exception {
    String pemStr = new String(pem, StandardCharsets.US_ASCII);
    boolean isRsaPkcs1 = pemStr.contains("RSA PRIVATE KEY");
    boolean isEc = pemStr.contains("EC PRIVATE KEY");
    String base64 = pemStr.replaceAll("-----BEGIN [^-]+-----", "")
                        .replaceAll("-----END [^-]+-----", "")
                        .replaceAll("\\s", "");
    byte[] der = Base64.getDecoder().decode(base64);
    String algo = "RSA";
    if (isRsaPkcs1) {
      der = convertPkcs1RsaToPkcs8(der);
    } else if (isEc) {
      algo = "EC";
    }
    PKCS8EncodedKeySpec spec = new PKCS8EncodedKeySpec(der);
    try {
      return KeyFactory.getInstance(algo).generatePrivate(spec);
    } catch (Exception e) {
      String alt = algo.equals("RSA") ? "EC" : "RSA";
      return KeyFactory.getInstance(alt).generatePrivate(spec);
    }
  }

  static java.security.PublicKey parsePublicKey(byte[] pem) throws Exception {
    String pemStr = new String(pem, StandardCharsets.US_ASCII);
    if (pemStr.contains("BEGIN CERTIFICATE")) {
      List<Certificate> certs = parseCertificates(pem);
      return certs.get(0).getPublicKey();
    }
    boolean isEc = pemStr.contains("EC PUBLIC KEY");
    String base64 = pemStr.replaceAll("-----BEGIN [^-]+-----", "")
                        .replaceAll("-----END [^-]+-----", "")
                        .replaceAll("\\s", "");
    byte[] der = Base64.getDecoder().decode(base64);
    java.security.spec.X509EncodedKeySpec spec =
        new java.security.spec.X509EncodedKeySpec(der);
    String algo = isEc ? "EC" : "RSA";
    try {
      return KeyFactory.getInstance(algo).generatePublic(spec);
    } catch (Exception e) {
      String alt = algo.equals("RSA") ? "EC" : "RSA";
      return KeyFactory.getInstance(alt).generatePublic(spec);
    }
  }

  private static List<Certificate> parseCertificates(byte[] pem)
      throws Exception {
    CertificateFactory cf = CertificateFactory.getInstance("X.509");
    List<Certificate> certs = new ArrayList<>();
    ByteArrayInputStream in = new ByteArrayInputStream(pem);
    while (in.available() > 0) {
      Certificate c = cf.generateCertificate(in);
      if (c == null)
        break;
      certs.add(c);
    }
    return certs;
  }

  public static SSLContext buildServerContext(byte[] keyPem, byte[] certPem)
      throws Exception {
    PrivateKey key = parsePrivateKey(keyPem);
    List<Certificate> certs = parseCertificates(certPem);
    KeyStore ks = KeyStore.getInstance("PKCS12");
    ks.load(null, null);
    char[] pass = "v6-internal".toCharArray();
    ks.setKeyEntry("server", key, pass, certs.toArray(new Certificate[0]));
    KeyManagerFactory kmf =
        KeyManagerFactory.getInstance(KeyManagerFactory.getDefaultAlgorithm());
    kmf.init(ks, pass);
    SSLContext ctx = SSLContext.getInstance("TLS");
    ctx.init(kmf.getKeyManagers(), null, null);
    return ctx;
  }

  public static SSLContext buildClientContext(boolean rejectUnauthorized)
      throws Exception {
    SSLContext ctx = SSLContext.getInstance("TLS");
    if (rejectUnauthorized) {
      ctx.init(null, null, null);
    } else {
      ctx.init(null, new javax.net.ssl.TrustManager[] {new V6TrustAllManager()},
               null);
    }
    return ctx;
  }
}
