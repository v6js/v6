import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.PrintStream;
import java.nio.charset.StandardCharsets;
import java.util.function.Consumer;

public final class V6IpcUtil {
  private V6IpcUtil() {
  }

  private static final String SENTINEL = "V6IPC";
  private static final V6Value UNDEF = new V6Value(V6Value.TAG_UNDEF, 0, null);

  public static void sendMessage(OutputStream out, V6Value msg) {
    try {
      String json = V6Json.stringify(msg, UNDEF, UNDEF).toString();
      out.write((SENTINEL + json + "\n").getBytes(StandardCharsets.UTF_8));
      out.flush();
    } catch (IOException ignored) {
    }
  }

  public static void pumpMessages(InputStream in, PrintStream passthroughOut,
                                  Consumer<V6Value> onMessage) {
    V6EventLoop.ref();
    Thread th = new Thread(() -> {
      try (BufferedReader r = new BufferedReader(
               new InputStreamReader(in, StandardCharsets.UTF_8))) {
        String line;
        while ((line = r.readLine()) != null) {
          if (line.startsWith(SENTINEL)) {
            String json = line.substring(SENTINEL.length());
            V6EventLoop.postExternal(() -> {
              try {
                onMessage.accept(V6Json.parse(json));
              } catch (RuntimeException ignored) {
              }
            });
          } else if (passthroughOut != null) {
            passthroughOut.println(line);
          }
        }
      } catch (IOException ignored) {
      } finally {
        V6EventLoop.unref();
      }
    });
    th.setDaemon(true);
    th.start();
  }

  public static String detectV6ExecutablePath() {
    try {
      String cmd = ProcessHandle.current().info().command().orElse(null);
      if (cmd == null)
        return null;
      return cmd.toLowerCase().contains("v6") ? cmd : null;
    } catch (Exception e) {
      return null;
    }
  }
}
