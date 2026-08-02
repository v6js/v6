public final class V6ResponseObject extends V6Object {
  int status = 200;
  String statusText = "";
  V6HeadersObject headers;
  byte[] bodyBytes = new byte[0];
  String url = "";
  boolean bodyUsed = false;
  boolean isError = false;
}
