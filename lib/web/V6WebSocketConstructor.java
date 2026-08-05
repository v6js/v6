import java.net.URI;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;

public final class V6WebSocketConstructor
    extends V6Object implements V6NativeConstructor {
  public static final V6Object PROTOTYPE = buildPrototype();

  public V6WebSocketConstructor() {
    set("prototype", new V6Value(V6Value.TAG_OBJ, 0, PROTOTYPE));
    PROTOTYPE.nativeCtor = this;
    set("CONNECTING", num(0));
    set("OPEN", num(1));
    set("CLOSING", num(2));
    set("CLOSED", num(3));
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

  private static byte[] bytesOf(V6Value v) {
    if (v.tag() == V6Value.TAG_OBJ && v.ref() instanceof V6Buffer)
      return ((V6Buffer)v.ref()).toBytes();
    if (v.tag() == V6Value.TAG_OBJ && v.ref() instanceof V6ArrayBufferObject)
      return ((V6ArrayBufferObject)v.ref()).data;
    return v.toString().getBytes(StandardCharsets.UTF_8);
  }

  @Override
  public V6Object allocate() {
    return new V6WebSocketObject();
  }

  @Override
  public V6Value construct(V6Value[] args) {
    V6WebSocketObject ws = new V6WebSocketObject();
    ws.setProto(PROTOTYPE);
    ws.url = V6Value.argAt(args, 0).toString();

    java.net.http.WebSocket.Builder builder =
        java.net.http.HttpClient.newHttpClient().newWebSocketBuilder();
    V6Value protocolsVal = V6Value.argAt(args, 1);
    if (protocolsVal.tag() == V6Value.TAG_STR) {
      builder.subprotocols(protocolsVal.toString());
    } else if (protocolsVal.tag() == V6Value.TAG_OBJ &&
               protocolsVal.ref() instanceof V6Array) {
      V6Array arr = (V6Array)protocolsVal.ref();
      if (arr.elemCount > 0) {
        String first = arr.elements[0].toString();
        String[] rest = new String[arr.elemCount - 1];
        for (int i = 1; i < arr.elemCount; i++)
          rest[i - 1] = arr.elements[i].toString();
        builder.subprotocols(first, rest);
      }
    }

    V6EventLoop.ref();
    Object capturedLoop = V6EventLoop.captureState();
    builder.buildAsync(URI.create(ws.url), new V6WebSocketListenerImpl(ws))
        .whenComplete((sock, err) -> {
          if (sock != null)
            ws.nativeSocket = sock;
          if (err != null) {
            V6EventLoop.postExternalTo(capturedLoop, () -> {
              ws.readyState = 3;
              V6EventObject e = new V6EventObject();
              e.setProto(V6EventConstructor.PROTOTYPE);
              e.type = "error";
              e.set("message", str(String.valueOf(err.getMessage())));
              ws.dispatch(e);
              V6EventObject closeEv = new V6EventObject();
              closeEv.setProto(V6EventConstructor.PROTOTYPE);
              closeEv.type = "close";
              ws.dispatch(closeEv);
              V6EventLoop.unrefCaptured(capturedLoop);
            });
          }
        });

    return objValue(ws);
  }

  @Override
  public V6Object prototypeObject() {
    return PROTOTYPE;
  }

  private static V6WebSocketObject self(V6Value t) {
    return (V6WebSocketObject)t.ref();
  }

  private static V6Object buildPrototype() {
    V6Object o = new V6Object();
    o.setProto(V6EventTargetConstructor.PROTOTYPE);

    o.defineGetter("readyState", (t, a) -> num(self(t).readyState));
    o.defineGetter("url", (t, a) -> str(self(t).url));
    o.defineGetter("bufferedAmount", (t, a) -> num(0));
    o.defineGetter("protocol", (t, a) -> {
      V6WebSocketObject ws = self(t);
      return ws.nativeSocket != null ? str(ws.nativeSocket.getSubprotocol())
                                     : str("");
    });
    o.defineGetter("binaryType", (t, a) -> str(self(t).binaryType));
    o.defineSetter("binaryType", (t, a) -> {
      self(t).binaryType = V6Value.argAt(a, 0).toString();
      return UNDEF;
    });

    o.set("send", fn((t, a) -> {
            V6WebSocketObject ws = self(t);
            if (ws.readyState != 1 || ws.nativeSocket == null)
              throw new V6Throw(
                  str("InvalidStateError: WebSocket is not open"));
            V6Value data = V6Value.argAt(a, 0);
            if (data.tag() == V6Value.TAG_STR) {
              ws.nativeSocket.sendText(data.toString(), true);
            } else {
              ws.nativeSocket.sendBinary(ByteBuffer.wrap(bytesOf(data)), true);
            }
            return UNDEF;
          }));

    o.set("close", fn((t, a) -> {
            V6WebSocketObject ws = self(t);
            int code = a.length > 0 && !a[0].isUndefined()
                           ? (int)a[0].toNumber()
                           : 1000;
            String reason =
                a.length > 1 && !a[1].isUndefined() ? a[1].toString() : "";
            if (ws.nativeSocket != null) {
              ws.readyState = 2;
              ws.nativeSocket.sendClose(code, reason);
            }
            return UNDEF;
          }));

    V6EventHandlerProperty.install(o, "onopen", "open");
    V6EventHandlerProperty.install(o, "onmessage", "message");
    V6EventHandlerProperty.install(o, "onclose", "close");
    V6EventHandlerProperty.install(o, "onerror", "error");

    return o;
  }
}
