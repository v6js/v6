import java.io.ByteArrayOutputStream;

public final class V6WebSocketObject extends V6EventTargetObject {
  java.net.http.WebSocket nativeSocket;
  int readyState = 0;
  String url = "";
  String binaryType = "blob";
  final StringBuilder textAccum = new StringBuilder();
  final ByteArrayOutputStream binaryAccum = new ByteArrayOutputStream();
}
