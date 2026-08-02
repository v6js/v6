import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;

public final class V6ChildProcess {
  private V6ChildProcess() {}

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
  private static final V6Value NUL = new V6Value(V6Value.TAG_NULL, 0, null);
  private static final boolean IS_WINDOWS =
      System.getProperty("os.name", "").toLowerCase().contains("win");

  private static byte[] bytesOf(V6Value v) {
    if (v.tag() == V6Value.TAG_OBJ && v.ref() instanceof V6Buffer)
      return ((V6Buffer)v.ref()).toBytes();
    return v.toString().getBytes(StandardCharsets.UTF_8);
  }

  private static List<String> shellCommand(String command) {
    List<String> cmd = new ArrayList<>();
    if (IS_WINDOWS) {
      cmd.add("cmd.exe");
      cmd.add("/c");
      cmd.add(command);
    } else {
      cmd.add("/bin/sh");
      cmd.add("-c");
      cmd.add(command);
    }
    return cmd;
  }

  private static List<String> argvCommand(String command, V6Value argsVal) {
    List<String> cmd = new ArrayList<>();
    cmd.add(command);
    if (argsVal.tag() == V6Value.TAG_OBJ && argsVal.ref() instanceof V6Array) {
      V6Array arr = (V6Array)argsVal.ref();
      int n = (int)arr.get("length").num();
      for (int i = 0; i < n; i++)
        cmd.add(arr.get(Integer.toString(i)).toString());
    }
    return cmd;
  }

  private static void pumpStream(java.io.InputStream in, V6Object target) {
    V6EventLoop.ref();
    Thread th = new Thread(() -> {
      try {
        byte[] buf = new byte[8192];
        int n;
        while ((n = in.read(buf)) != -1) {
          byte[] chunk = java.util.Arrays.copyOf(buf, n);
          V6EventLoop.postExternal(
              ()
                  -> target.get("push").asCallable().call(
                      objValue(target), new V6Value[] {objValue(new V6Buffer(chunk))}));
        }
      } catch (IOException ignored) {
      } finally {
        V6EventLoop.postExternal(
            ()
                -> target.get("push").asCallable().call(objValue(target),
                                                        new V6Value[] {NUL}));
        V6EventLoop.unref();
      }
    });
    th.setDaemon(true);
    th.start();
  }

  private static void waitForExit(Process proc, V6Object cp) {
    V6EventLoop.ref();
    Thread th = new Thread(() -> {
      try {
        int code = proc.waitFor();
        V6EventLoop.postExternal(() -> {
          cp.get("emit").asCallable().call(
              objValue(cp),
              new V6Value[] {str("exit"), new V6Value(V6Value.TAG_NUM, code, null), NUL});
          cp.get("emit").asCallable().call(
              objValue(cp),
              new V6Value[] {str("close"), new V6Value(V6Value.TAG_NUM, code, null), NUL});
        });
      } catch (InterruptedException ignored) {
      } finally {
        V6EventLoop.unref();
      }
    });
    th.setDaemon(true);
    th.start();
  }

  public static V6Object build() {
    V6Object o = new V6Object();

    o.set("spawn", fn((thisArg, args) -> {
            String command = V6Value.argAt(args, 0).toString();
            V6Value argsVal = V6Value.argAt(args, 1);
            V6Object options = null;
            for (V6Value a : args)
              if (a.tag() == V6Value.TAG_OBJ && !(a.ref() instanceof V6Array))
                options = (V6Object)a.ref();
            boolean shell = options != null && options.get("shell").truthy();

            List<String> cmd = shell ? shellCommand(command) : argvCommand(command, argsVal);
            ProcessBuilder pb = new ProcessBuilder(cmd);
            if (options != null) {
              V6Value cwdVal = options.get("cwd");
              if (cwdVal.tag() == V6Value.TAG_STR)
                pb.directory(new java.io.File(cwdVal.toString()));
            }

            V6EventEmitterObject cp = new V6EventEmitterObject();
            cp.setProto(V6EventEmitterConstructor.PROTOTYPE);

            Process proc;
            try {
              proc = pb.start();
            } catch (IOException e) {
              V6MicrotaskQueue.enqueue(
                  ()
                      -> cp.get("emit").asCallable().call(
                          objValue(cp),
                          new V6Value[] {str("error"), str(String.valueOf(e.getMessage()))}));
              return objValue(cp);
            }

            cp.set("pid", new V6Value(V6Value.TAG_NUM, proc.pid(), null));

            V6EventEmitterObject stdout = new V6EventEmitterObject();
            stdout.setProto(V6StreamReadableConstructor.PROTOTYPE);
            V6EventEmitterObject stderr = new V6EventEmitterObject();
            stderr.setProto(V6StreamReadableConstructor.PROTOTYPE);
            V6EventEmitterObject stdin = new V6EventEmitterObject();
            stdin.setProto(V6StreamWritableConstructor.PROTOTYPE);
            cp.set("stdout", objValue(stdout));
            cp.set("stderr", objValue(stderr));
            cp.set("stdin", objValue(stdin));

            final Process fproc = proc;
            stdin.set("_write", fn((t, a2) -> {
                        try {
                          fproc.getOutputStream().write(bytesOf(V6Value.argAt(a2, 0)));
                          fproc.getOutputStream().flush();
                        } catch (IOException ignored) {
                        }
                        V6Callable cb = a2.length > 2 && a2[2].tag() == V6Value.TAG_FUNC
                            ? a2[2].asCallable()
                            : null;
                        if (cb != null)
                          cb.call(UNDEF, new V6Value[0]);
                        return UNDEF;
                      }));

            pumpStream(proc.getInputStream(), stdout);
            pumpStream(proc.getErrorStream(), stderr);
            waitForExit(proc, cp);

            cp.set("kill", fn((t, a2) -> {
                    fproc.destroy();
                    return new V6Value(V6Value.TAG_BOOL, 1, null);
                  }));

            return objValue(cp);
          }));

    o.set("exec", fn((thisArg, args) -> {
            String command = V6Value.argAt(args, 0).toString();
            V6Callable cb = null;
            for (int i = 1; i < args.length; i++)
              if (args[i].tag() == V6Value.TAG_FUNC) {
                cb = args[i].asCallable();
                break;
              }
            final V6Callable fcb = cb;
            List<String> cmd = shellCommand(command);
            ProcessBuilder pb = new ProcessBuilder(cmd);
            V6EventLoop.ref();
            Thread th = new Thread(() -> {
              try {
                Process proc = pb.start();
                byte[] out = proc.getInputStream().readAllBytes();
                byte[] err = proc.getErrorStream().readAllBytes();
                int code = proc.waitFor();
                V6EventLoop.postExternal(() -> {
                  if (fcb != null) {
                    V6Value errVal =
                        code == 0 ? NUL : str("Command failed: " + command);
                    fcb.call(UNDEF,
                            new V6Value[] {errVal, str(new String(out, StandardCharsets.UTF_8)),
                                          str(new String(err, StandardCharsets.UTF_8))});
                  }
                });
              } catch (Exception e) {
                V6EventLoop.postExternal(() -> {
                  if (fcb != null)
                    fcb.call(UNDEF,
                            new V6Value[] {str(String.valueOf(e.getMessage())), str(""), str("")});
                });
              } finally {
                V6EventLoop.unref();
              }
            });
            th.setDaemon(true);
            th.start();
            return UNDEF;
          }));

    o.set("execSync", fn((thisArg, args) -> {
            String command = V6Value.argAt(args, 0).toString();
            List<String> cmd = shellCommand(command);
            try {
              ProcessBuilder pb = new ProcessBuilder(cmd);
              Process proc = pb.start();
              byte[] out = proc.getInputStream().readAllBytes();
              byte[] err = proc.getErrorStream().readAllBytes();
              int code = proc.waitFor();
              if (code != 0)
                throw new V6Throw(str("Command failed: " + command + "\n" +
                                      new String(err, StandardCharsets.UTF_8)));
              return objValue(new V6Buffer(out));
            } catch (IOException | InterruptedException e) {
              throw new V6Throw(str("execSync failed: " + e.getMessage()));
            }
          }));

    o.set("spawnSync", fn((thisArg, args) -> {
            String command = V6Value.argAt(args, 0).toString();
            V6Value argsVal = V6Value.argAt(args, 1);
            List<String> cmd = argvCommand(command, argsVal);
            V6Object result = new V6Object();
            try {
              ProcessBuilder pb = new ProcessBuilder(cmd);
              Process proc = pb.start();
              byte[] out = proc.getInputStream().readAllBytes();
              byte[] err = proc.getErrorStream().readAllBytes();
              int code = proc.waitFor();
              result.set("status", new V6Value(V6Value.TAG_NUM, code, null));
              result.set("stdout", objValue(new V6Buffer(out)));
              result.set("stderr", objValue(new V6Buffer(err)));
            } catch (Exception e) {
              result.set("status", NUL);
              result.set("error", str(String.valueOf(e.getMessage())));
            }
            return objValue(result);
          }));

    return o;
  }
}
