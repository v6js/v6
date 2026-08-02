public final class V6EventSourceObject extends V6EventTargetObject {
  int readyState = 0;
  String url = "";
  String lastEventId = "";
  volatile boolean closed = false;
  volatile java.io.InputStream currentStream;
  volatile long retryMs = 3000;
}
