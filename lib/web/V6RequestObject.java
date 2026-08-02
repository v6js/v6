public final class V6RequestObject extends V6Object {
  String url = "";
  String method = "GET";
  V6HeadersObject headers;
  byte[] bodyBytes = new byte[0];
  V6Value signal = new V6Value(V6Value.TAG_UNDEF, 0, null);
  boolean bodyUsed = false;
}
