public final class V6TrustAllManager implements javax.net.ssl.X509TrustManager {
  @Override
  public java.security.cert.X509Certificate[] getAcceptedIssuers() {
    return new java.security.cert.X509Certificate[0];
  }

  @Override
  public void checkClientTrusted(java.security.cert.X509Certificate[] chain,
                                 String authType) {
  }

  @Override
  public void checkServerTrusted(java.security.cert.X509Certificate[] chain,
                                 String authType) {
  }
}
