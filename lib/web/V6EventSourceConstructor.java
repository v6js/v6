import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.net.URI;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.nio.charset.StandardCharsets;

public final class V6EventSourceConstructor
    extends V6Object implements V6NativeConstructor {
  public static final V6Object PROTOTYPE = buildPrototype();

  public V6EventSourceConstructor() {
    set("prototype", new V6Value(V6Value.TAG_OBJ, 0, PROTOTYPE));
    PROTOTYPE.nativeCtor = this;
    set("CONNECTING", num(0));
    set("OPEN", num(1));
    set("CLOSED", num(2));
  }

  private static V6Value str(String s) {
    return new V6Value(V6Value.TAG_STR, 0, s);
  }

  private static V6Value objValue(V6Object o) {
    return new V6Value(V6Value.TAG_OBJ, 0, o);
  }

  private static V6Value num(double n) {
    return new V6Value(V6Value.TAG_NUM, n, null);
  }

  private static V6Value fn(V6Callable c) {
    return new V6Value(V6Value.TAG_FUNC, 0, c);
  }

  private static final V6Value UNDEF = new V6Value(V6Value.TAG_UNDEF, 0, null);

  private static V6EventObject newEvent(String type) {
    V6EventObject e = new V6EventObject();
    e.setProto(V6EventConstructor.PROTOTYPE);
    e.type = type;
    return e;
  }

  private static void scheduleReconnect(V6EventSourceObject es) {
    if (es.closed)
      return;
    es.readyState = 0;
    V6EventLoop.schedule((t, a) -> {
      connect(es);
      return UNDEF;
    }, es.retryMs, 0, new V6Value[0]);
  }

  private static void connect(V6EventSourceObject es) {
    if (es.closed)
      return;
    HttpRequest.Builder builder = HttpRequest.newBuilder(URI.create(es.url))
                                      .header("Accept", "text/event-stream");
    if (!es.lastEventId.isEmpty())
      builder.header("Last-Event-ID", es.lastEventId);
    HttpRequest req = builder.build();

    V6EventLoop.ref();
    V6Http.DEFAULT_CLIENT
        .sendAsync(req, HttpResponse.BodyHandlers.ofInputStream())
        .whenComplete((resp, err) -> {
          if (err != null) {
            V6EventLoop.postExternal(() -> {
              es.dispatch(newEvent("error"));
              scheduleReconnect(es);
              V6EventLoop.unref();
            });
            return;
          }
          if (resp.statusCode() < 200 || resp.statusCode() >= 300) {
            V6EventLoop.postExternal(() -> {
              es.dispatch(newEvent("error"));
              scheduleReconnect(es);
              V6EventLoop.unref();
            });
            return;
          }
          es.currentStream = resp.body();
          V6EventLoop.postExternal(() -> {
            es.readyState = 1;
            es.dispatch(newEvent("open"));
          });
          readEvents(es, resp.body());
        });
  }

  private static void readEvents(V6EventSourceObject es,
                                 java.io.InputStream body) {
    Thread th = new Thread(() -> {
      try (BufferedReader r = new BufferedReader(
               new InputStreamReader(body, StandardCharsets.UTF_8))) {
        StringBuilder dataBuf = new StringBuilder();
        String[] eventTypeHolder = {"message"};
        String line;
        while (!es.closed && (line = r.readLine()) != null) {
          if (line.isEmpty()) {
            if (dataBuf.length() > 0) {
              String data = dataBuf.charAt(dataBuf.length() - 1) == '\n'
                                ? dataBuf.substring(0, dataBuf.length() - 1)
                                : dataBuf.toString();
              String type = eventTypeHolder[0];
              V6EventLoop.postExternal(() -> {
                V6EventObject e = newEvent(type);
                e.set("data", str(data));
                e.set("lastEventId", str(es.lastEventId));
                es.dispatch(e);
              });
              dataBuf.setLength(0);
            }
            eventTypeHolder[0] = "message";
            continue;
          }
          if (line.startsWith(":"))
            continue;
          int idx = line.indexOf(':');
          String field = idx < 0 ? line : line.substring(0, idx);
          String value;
          if (idx < 0)
            value = "";
          else {
            String rest = line.substring(idx + 1);
            value = rest.startsWith(" ") ? rest.substring(1) : rest;
          }
          switch (field) {
          case "data":
            dataBuf.append(value).append('\n');
            break;
          case "event":
            eventTypeHolder[0] = value;
            break;
          case "id":
            es.lastEventId = value;
            break;
          case "retry":
            try {
              es.retryMs = Long.parseLong(value.trim());
            } catch (NumberFormatException ignored) {
            }
            break;
          default:
            break;
          }
        }
      } catch (IOException ignored) {
      } finally {
        V6EventLoop.postExternal(() -> {
          if (!es.closed) {
            es.dispatch(newEvent("error"));
            scheduleReconnect(es);
          }
          V6EventLoop.unref();
        });
      }
    });
    th.setDaemon(true);
    th.start();
  }

  @Override
  public V6Object allocate() {
    return new V6EventSourceObject();
  }

  @Override
  public V6Value construct(V6Value[] args) {
    V6EventSourceObject es = new V6EventSourceObject();
    es.setProto(PROTOTYPE);
    es.url = V6Value.argAt(args, 0).toString();
    connect(es);
    return objValue(es);
  }

  @Override
  public V6Object prototypeObject() {
    return PROTOTYPE;
  }

  private static V6EventSourceObject self(V6Value t) {
    return (V6EventSourceObject)t.ref();
  }

  private static V6Object buildPrototype() {
    V6Object o = new V6Object();
    o.setProto(V6EventTargetConstructor.PROTOTYPE);

    o.defineGetter("readyState", (t, a) -> num(self(t).readyState));
    o.defineGetter("url", (t, a) -> str(self(t).url));
    o.defineGetter("withCredentials",
                   (t, a) -> new V6Value(V6Value.TAG_BOOL, 0, null));

    o.set("close", fn((t, a) -> {
            V6EventSourceObject es = self(t);
            es.closed = true;
            es.readyState = 2;
            if (es.currentStream != null) {
              try {
                es.currentStream.close();
              } catch (IOException ignored) {
              }
            }
            return UNDEF;
          }));

    V6EventHandlerProperty.install(o, "onopen", "open");
    V6EventHandlerProperty.install(o, "onmessage", "message");
    V6EventHandlerProperty.install(o, "onerror", "error");

    return o;
  }
}
