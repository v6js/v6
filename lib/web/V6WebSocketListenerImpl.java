import java.net.http.WebSocket;
import java.nio.ByteBuffer;

public final class V6WebSocketListenerImpl implements WebSocket.Listener {
  private final V6WebSocketObject socket;

  public V6WebSocketListenerImpl(V6WebSocketObject socket) {
    this.socket = socket;
  }

  private static V6Value str(String s) {
    return new V6Value(V6Value.TAG_STR, 0, s);
  }

  private static V6Value objValue(V6Object o) {
    return new V6Value(V6Value.TAG_OBJ, 0, o);
  }

  private V6EventObject newEvent(String type) {
    V6EventObject e = new V6EventObject();
    e.setProto(V6EventConstructor.PROTOTYPE);
    e.type = type;
    return e;
  }

  @Override
  public void onOpen(WebSocket webSocket) {
    V6EventLoop.postExternal(() -> {
      socket.readyState = 1;
      socket.dispatch(newEvent("open"));
    });
    webSocket.request(1);
  }

  @Override
  public java.util.concurrent.CompletionStage<?>
  onText(WebSocket webSocket, CharSequence data, boolean last) {
    socket.textAccum.append(data);
    if (last) {
      String full = socket.textAccum.toString();
      socket.textAccum.setLength(0);
      V6EventLoop.postExternal(() -> {
        V6EventObject e = newEvent("message");
        e.set("data", str(full));
        socket.dispatch(e);
      });
    }
    webSocket.request(1);
    return null;
  }

  @Override
  public java.util.concurrent.CompletionStage<?>
  onBinary(WebSocket webSocket, ByteBuffer data, boolean last) {
    byte[] chunk = new byte[data.remaining()];
    data.get(chunk);
    socket.binaryAccum.write(chunk, 0, chunk.length);
    if (last) {
      byte[] full = socket.binaryAccum.toByteArray();
      socket.binaryAccum.reset();
      V6EventLoop.postExternal(() -> {
        V6EventObject e = newEvent("message");
        V6Value payload = socket.binaryType.equals("arraybuffer")
                              ? V6ArrayBufferConstructor.wrap(full)
                              : objValue(new V6Buffer(full));
        e.set("data", payload);
        socket.dispatch(e);
      });
    }
    webSocket.request(1);
    return null;
  }

  @Override
  public java.util.concurrent.CompletionStage<?>
  onClose(WebSocket webSocket, int statusCode, String reason) {
    V6EventLoop.postExternal(() -> {
      socket.readyState = 3;
      V6EventObject e = newEvent("close");
      e.set("code", new V6Value(V6Value.TAG_NUM, statusCode, null));
      e.set("reason", str(reason == null ? "" : reason));
      e.set("wasClean", new V6Value(V6Value.TAG_BOOL, 1, null));
      socket.dispatch(e);
      V6EventLoop.unref();
    });
    return null;
  }

  @Override
  public void onError(WebSocket webSocket, Throwable error) {
    V6EventLoop.postExternal(() -> {
      socket.readyState = 3;
      V6EventObject e = newEvent("error");
      e.set("message", str(String.valueOf(error.getMessage())));
      socket.dispatch(e);
      socket.dispatch(newEvent("close"));
      V6EventLoop.unref();
    });
  }
}
