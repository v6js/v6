import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.ArrayDeque;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.Map;

public final class V6Daemon {
  private V6Daemon() {
  }

  private static final byte TAG_STDOUT = 1;
  private static final byte TAG_STDERR = 2;
  private static final byte TAG_EXIT = 3;

  private static final int MAX_CACHED_SCRIPTS = 64;
  private static final Map<String, Class<?>> scriptClassCache = new HashMap<>();
  private static final ArrayDeque<String> scriptClassCacheOrder =
      new ArrayDeque<>();
  private static final Object scriptClassCacheLock = new Object();

  private static String hashClassBytes(byte[] bytes) {
    long h = 0xcbf29ce484222325L;
    for (byte b : bytes) {
      h ^= (b & 0xff);
      h *= 0x100000001b3L;
    }
    return Long.toHexString(h) + ":" + bytes.length;
  }

  private static String readString(DataInputStream in) throws IOException {
    int len = in.readInt();
    byte[] buf = new byte[len];
    in.readFully(buf);
    return new String(buf, StandardCharsets.UTF_8);
  }

  private static byte[] readBytes(DataInputStream in) throws IOException {
    int len = in.readInt();
    byte[] buf = new byte[len];
    in.readFully(buf);
    return buf;
  }

  private static void writeLockFile(String lockFilePath, long pid, int port,
                                    long binaryMtime, long binarySize)
      throws IOException {
    Path target = Path.of(lockFilePath);
    Path tmp = Path.of(lockFilePath + ".tmp-" + pid);
    String content =
        pid + "\n" + port + "\n" + binaryMtime + "\n" + binarySize + "\n";
    Files.writeString(tmp, content, StandardCharsets.UTF_8);
    Files.move(tmp, target, StandardCopyOption.REPLACE_EXISTING,
              StandardCopyOption.ATOMIC_MOVE);
  }

  private static boolean handleConnection(Socket socket,
                                          ClassLoader parentLoader,
                                          java.io.PrintStream realOut,
                                          java.io.PrintStream realErr,
                                          long executionTimeoutMillis) {
    try (Socket s = socket) {
      s.setTcpNoDelay(true);
      s.setSoTimeout(15000);
      DataInputStream in = new DataInputStream(
          new java.io.BufferedInputStream(s.getInputStream()));
      DataOutputStream out =
          new DataOutputStream(new java.io.BufferedOutputStream(
              s.getOutputStream(), 8192));

      int numClasses = in.readInt();
      String[] classNames = new String[numClasses];
      byte[][] classBytes = new byte[numClasses][];
      for (int i = 0; i < numClasses; i++) {
        classNames[i] = readString(in);
        classBytes[i] = readBytes(in);
      }

      Class<?> mainClass = null;
      if (numClasses == 1 && classNames[0].equals("Main")) {
        String cacheKey = hashClassBytes(classBytes[0]);
        synchronized (scriptClassCacheLock) {
          mainClass = scriptClassCache.get(cacheKey);
        }
        if (mainClass == null) {
          V6DaemonClassLoader loader = new V6DaemonClassLoader(parentLoader);
          mainClass = loader.defineFromBytes(classNames[0], classBytes[0]);
          synchronized (scriptClassCacheLock) {
            scriptClassCache.put(cacheKey, mainClass);
            scriptClassCacheOrder.addLast(cacheKey);
            if (scriptClassCacheOrder.size() > MAX_CACHED_SCRIPTS) {
              String evictKey = scriptClassCacheOrder.pollFirst();
              scriptClassCache.remove(evictKey);
            }
          }
        }
      } else {
        V6DaemonClassLoader loader = new V6DaemonClassLoader(parentLoader);
        for (int i = 0; i < numClasses; i++) {
          Class<?> cls = loader.defineFromBytes(classNames[i], classBytes[i]);
          if (classNames[i].equals("Main"))
            mainClass = cls;
        }
      }

      String scriptPath = readString(in);
      int argc = in.readInt();
      String[] args = new String[argc];
      for (int i = 0; i < argc; i++)
        args[i] = readString(in);

      String cwd = readString(in);
      int envCount = in.readInt();
      Map<String, String> env = new LinkedHashMap<>();
      for (int i = 0; i < envCount; i++) {
        String k = readString(in);
        String v = readString(in);
        env.put(k, v);
      }

      if (mainClass == null) {
        out.writeByte(TAG_EXIT);
        out.writeInt(-1);
        out.flush();
        return false;
      }

      s.setSoTimeout(0);

      V6EventLoop.reset();
      V6MicrotaskQueue.reset();
      V6Builtins.resetGlobalObject();
      V6Process.resetForRequest(env, cwd);

      Object lock = new Object();
      java.io.PrintStream taggedOut = new java.io.PrintStream(
          new V6TaggedStream(out, TAG_STDOUT, lock), true,
          StandardCharsets.UTF_8);
      java.io.PrintStream taggedErr = new java.io.PrintStream(
          new V6TaggedStream(out, TAG_STDERR, lock), true,
          StandardCharsets.UTF_8);

      Method m;
      try {
        m = mainClass.getMethod("main", String[].class);
        m.setAccessible(true);
      } catch (ReflectiveOperationException e) {
        taggedErr.println("error: daemon failed to invoke entry point: " +
                          e.getMessage());
        synchronized (lock) {
          out.writeByte(TAG_EXIT);
          out.writeInt(1);
          out.flush();
        }
        return false;
      }

      int[] exitCodeHolder = {0};
      Method entryMethod = m;
      Thread worker = new Thread(() -> {
        System.setOut(taggedOut);
        System.setErr(taggedErr);
        try {
          entryMethod.invoke(null, (Object)args);
        } catch (InvocationTargetException ite) {
          Throwable cause = ite.getCause();
          if (cause instanceof V6ProcessExit) {
            exitCodeHolder[0] = ((V6ProcessExit)cause).code;
          } else if (cause instanceof V6Throw) {
            V6Value v = ((V6Throw)cause).value;
            taggedErr.println("Uncaught " + v.toString());
            exitCodeHolder[0] = 1;
          } else {
            if (cause != null)
              cause.printStackTrace(taggedErr);
            exitCodeHolder[0] = 1;
          }
        } catch (Throwable t) {
          t.printStackTrace(taggedErr);
          exitCodeHolder[0] = 1;
        } finally {
          taggedOut.flush();
          taggedErr.flush();
        }
      }, "v6-request-worker");
      worker.setDaemon(true);
      worker.start();
      try {
        worker.join(executionTimeoutMillis);
      } catch (InterruptedException ignored) {
        Thread.currentThread().interrupt();
      }

      System.setOut(realOut);
      System.setErr(realErr);

      if (worker.isAlive()) {
        try {
          synchronized (lock) {
            taggedErr.println("error: script execution exceeded " +
                              executionTimeoutMillis +
                              "ms; daemon restarting");
            out.writeByte(TAG_EXIT);
            out.writeInt(124);
            out.flush();
          }
        } catch (IOException ignored) {
        }
        return true;
      }

      synchronized (lock) {
        out.writeByte(TAG_EXIT);
        out.writeInt(exitCodeHolder[0]);
        out.flush();
      }
      return false;
    } catch (IOException e) {
      return false;
    }
  }

  public static void serve(String lockFilePath, long idleTimeoutMillis,
                           long binaryMtime, long binarySize,
                           long executionTimeoutMillis) {
    java.io.PrintStream realOut = System.out;
    java.io.PrintStream realErr = System.err;
    ClassLoader parentLoader = V6Daemon.class.getClassLoader();
    V6EventLoop.setIgnoredThread(Thread.currentThread());

    try (ServerSocket server =
             new ServerSocket(0, 64, InetAddress.getLoopbackAddress())) {
      server.setSoTimeout(5000);
      long pid = ProcessHandle.current().pid();
      writeLockFile(lockFilePath, pid, server.getLocalPort(), binaryMtime,
                    binarySize);

      long lastActivity = System.currentTimeMillis();
      while (true) {
        Socket socket;
        try {
          socket = server.accept();
        } catch (java.net.SocketTimeoutException te) {
          if (System.currentTimeMillis() - lastActivity > idleTimeoutMillis)
            break;
          continue;
        }
        lastActivity = System.currentTimeMillis();
        boolean wedged = handleConnection(socket, parentLoader, realOut,
                                          realErr, executionTimeoutMillis);
        if (wedged) {
          try {
            Files.deleteIfExists(Path.of(lockFilePath));
          } catch (IOException ignored) {
          }
          Runtime.getRuntime().halt(1);
        }
      }
    } catch (IOException e) {
      realErr.println("v6 daemon: fatal: " + e.getMessage());
    } finally {
      try {
        Files.deleteIfExists(Path.of(lockFilePath));
      } catch (IOException ignored) {
      }
    }
  }
}
