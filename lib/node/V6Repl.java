import java.io.BufferedReader;
import java.io.File;
import java.io.IOException;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;

public final class V6Repl {
  private V6Repl() {
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

  private static final V6Value UNDEF = new V6Value(V6Value.TAG_UNDEF, 0, null);

  private static final String[] STATEMENT_PREFIXES = {
      "const ",  "let ",    "var ", "function ", "function*", "class ",
      "if ",     "if(",     "for ", "for(",      "while ",    "while(",
      "switch ", "switch(", "try ", "try{",      "import ",   "export ",
      "return ", "return;", "{",    "//",
  };

  private static boolean looksLikeStatement(String line) {
    for (String p : STATEMENT_PREFIXES) {
      if (line.startsWith(p))
        return true;
    }
    return false;
  }

  private static String wrapLine(String line) {
    if (looksLikeStatement(line))
      return line;
    String expr =
        line.endsWith(";") ? line.substring(0, line.length() - 1) : line;
    return "console.log(" + expr + ");";
  }

  public static V6Object start(V6Value[] args) {
    V6Value first = V6Value.argAt(args, 0);
    String[] promptHolder = {"> "};
    if (first.tag() == V6Value.TAG_OBJ && first.ref() instanceof V6Object) {
      V6Value p = ((V6Object)first.ref()).get("prompt");
      if (p.tag() == V6Value.TAG_STR)
        promptHolder[0] = p.toString();
    }

    V6EventEmitterObject rl = new V6EventEmitterObject();
    rl.setProto(V6EventEmitterConstructor.PROTOTYPE);

    V6Object context = new V6Object();
    rl.set("context", objValue(context));

    rl.set("setPrompt", fn((t, a) -> {
             promptHolder[0] = V6Value.argAt(a, 0).toString();
             return UNDEF;
           }));

    rl.set("prompt", fn((t, a) -> {
             System.out.print(promptHolder[0]);
             System.out.flush();
             return UNDEF;
           }));

    rl.set("defineCommand", fn((t, a) -> UNDEF));

    boolean[] closed = {false};
    rl.set("close", fn((t, a) -> {
             closed[0] = true;
             rl.get("emit").asCallable().call(objValue(rl),
                                              new V6Value[] {str("close")});
             return UNDEF;
           }));

    String exePath = V6IpcUtil.detectV6ExecutablePath();
    if (exePath == null) {
      throw new V6Throw(str("repl.start is not supported when running via "
                            + "'java -jar' (AOT-jar mode "
                            + "embeds only the one already-compiled program, "
                            + "with no compiler available "
                            + "to evaluate REPL input); run via the v6 "
                            + "executable directly to use repl"));
    }

    V6EventLoop.ref();
    Thread th = new Thread(() -> runLoop(exePath, rl, promptHolder, closed));
    th.setDaemon(true);
    th.start();

    return rl;
  }

  private static void runLoop(String exePath, V6EventEmitterObject rl,
                              String[] promptHolder, boolean[] closed) {
    StringBuilder accumulated = new StringBuilder();
    long[] prevLen = {0};
    try {
      System.out.print(promptHolder[0]);
      System.out.flush();
      BufferedReader r = new BufferedReader(
          new InputStreamReader(System.in, StandardCharsets.UTF_8));
      String line;
      while (!closed[0] && (line = r.readLine()) != null) {
        String trimmed = line.trim();
        if (trimmed.isEmpty()) {
          System.out.print(promptHolder[0]);
          System.out.flush();
          continue;
        }
        if (trimmed.equals(".exit"))
          break;

        String wrapped = wrapLine(trimmed);
        String fullSrc = accumulated.toString() + wrapped + "\n";
        File tmp = File.createTempFile("v6repl", ".js");
        tmp.deleteOnExit();
        Files.write(tmp.toPath(), fullSrc.getBytes(StandardCharsets.UTF_8));

        ProcessBuilder pb = new ProcessBuilder(exePath, tmp.getAbsolutePath());
        pb.redirectErrorStream(true);
        Process proc = pb.start();
        String output = new String(proc.getInputStream().readAllBytes(),
                                   StandardCharsets.UTF_8);
        int exitCode = proc.waitFor();
        tmp.delete();

        String newPart = output.length() > prevLen[0]
                             ? output.substring((int)prevLen[0])
                             : "";
        System.out.print(newPart);
        System.out.flush();

        if (exitCode == 0) {
          accumulated.append(wrapped).append("\n");
          prevLen[0] = output.length();
        }

        final String lineFinal = trimmed;
        V6EventLoop.postExternal(
            ()
                -> rl.get("emit").asCallable().call(
                    objValue(rl), new V6Value[] {str("line"), str(lineFinal)}));

        if (!closed[0]) {
          System.out.print(promptHolder[0]);
          System.out.flush();
        }
      }
    } catch (IOException | InterruptedException ignored) {
    } finally {
      V6EventLoop.postExternal(
          ()
              -> rl.get("emit").asCallable().call(
                  objValue(rl), new V6Value[] {str("close")}));
      V6EventLoop.unref();
    }
  }

  public static V6Object build() {
    V6Object o = new V6Object();
    o.set("start", fn((thisArg, args) -> objValue(start(args))));
    return o;
  }
}
