import java.io.IOException;
import java.nio.file.DirectoryStream;
import java.nio.file.Files;
import java.nio.file.LinkOption;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.attribute.BasicFileAttributes;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;

public final class V6Fs {
  private V6Fs() {}

  private static V6Value str(String s) {
    return new V6Value(V6Value.TAG_STR, 0, s);
  }

  private static V6Value num(double n) {
    return new V6Value(V6Value.TAG_NUM, n, null);
  }

  private static V6Value bool(boolean b) {
    return new V6Value(V6Value.TAG_BOOL, b ? 1 : 0, null);
  }

  private static V6Value fn(V6Callable c) {
    return new V6Value(V6Value.TAG_FUNC, 0, c);
  }

  private static V6Value objValue(V6Object o) {
    return new V6Value(V6Value.TAG_OBJ, 0, o);
  }

  private static final V6Value UNDEF = new V6Value(V6Value.TAG_UNDEF, 0, null);
  private static final V6Value NUL = new V6Value(V6Value.TAG_NULL, 0, null);

  private static RuntimeException ioError(String op, String path, Exception cause) {
    return new V6Throw(str(op + " '" + path + "': " + cause.getMessage()));
  }

  private static String encodingOf(V6Value[] args, int idx) {
    if (args.length <= idx)
      return null;
    V6Value v = args[idx];
    if (v.tag() == V6Value.TAG_STR)
      return v.toString();
    if (v.tag() == V6Value.TAG_OBJ) {
      V6Value enc = v.ref() instanceof V6Object ? ((V6Object)v.ref()).get("encoding") : UNDEF;
      return enc.tag() == V6Value.TAG_STR ? enc.toString() : null;
    }
    return null;
  }

  private static V6Object statsObject(Path p) throws IOException {
    BasicFileAttributes attrs = Files.readAttributes(p, BasicFileAttributes.class,
                                                      LinkOption.NOFOLLOW_LINKS);
    boolean isFile = attrs.isRegularFile();
    boolean isDir = attrs.isDirectory();
    V6Object o = new V6Object();
    o.set("size", num(attrs.size()));
    o.set("mtimeMs", num(attrs.lastModifiedTime().toMillis()));
    o.set("ctimeMs", num(attrs.creationTime().toMillis()));
    o.set("birthtimeMs", num(attrs.creationTime().toMillis()));
    o.set("isFile", fn((t, a) -> bool(isFile)));
    o.set("isDirectory", fn((t, a) -> bool(isDir)));
    o.set("isSymbolicLink", fn((t, a) -> bool(attrs.isSymbolicLink())));
    return o;
  }

  private static void deferCallback(V6Callable cb, V6Value err, V6Value result) {
    V6MicrotaskQueue.enqueue(() -> cb.call(UNDEF, new V6Value[] {err, result}));
  }

  public static V6Object build() {
    V6Object o = new V6Object();

    o.set("existsSync", fn((thisArg, args) -> {
            Path p = Paths.get(V6Value.argAt(args, 0).toString());
            return bool(Files.exists(p));
          }));

    o.set("readFileSync", fn((thisArg, args) -> {
            String pathStr = V6Value.argAt(args, 0).toString();
            String encoding = encodingOf(args, 1);
            try {
              byte[] bytes = Files.readAllBytes(Paths.get(pathStr));
              if (encoding != null)
                return str(V6BufferConstructor.encodeBytes(bytes, encoding));
              return objValue(new V6Buffer(bytes));
            } catch (IOException e) {
              throw ioError("ENOENT: no such file or directory, open", pathStr, e);
            }
          }));

    o.set("writeFileSync", fn((thisArg, args) -> {
            String pathStr = V6Value.argAt(args, 0).toString();
            byte[] bytes = toBytesForWrite(V6Value.argAt(args, 1), encodingOf(args, 2));
            try {
              Files.write(Paths.get(pathStr), bytes);
            } catch (IOException e) {
              throw ioError("ENOENT: could not write file", pathStr, e);
            }
            return UNDEF;
          }));

    o.set("appendFileSync", fn((thisArg, args) -> {
            String pathStr = V6Value.argAt(args, 0).toString();
            byte[] bytes = toBytesForWrite(V6Value.argAt(args, 1), encodingOf(args, 2));
            try {
              Files.write(Paths.get(pathStr), bytes, java.nio.file.StandardOpenOption.CREATE,
                         java.nio.file.StandardOpenOption.APPEND);
            } catch (IOException e) {
              throw ioError("ENOENT: could not append to file", pathStr, e);
            }
            return UNDEF;
          }));

    o.set("mkdirSync", fn((thisArg, args) -> {
            String pathStr = V6Value.argAt(args, 0).toString();
            boolean recursive = false;
            if (args.length > 1 && args[1].tag() == V6Value.TAG_OBJ) {
              V6Value r = ((V6Object)args[1].ref()).get("recursive");
              recursive = r.truthy();
            }
            try {
              if (recursive)
                Files.createDirectories(Paths.get(pathStr));
              else
                Files.createDirectory(Paths.get(pathStr));
            } catch (IOException e) {
              throw ioError("EEXIST: could not create directory", pathStr, e);
            }
            return UNDEF;
          }));

    o.set("readdirSync", fn((thisArg, args) -> {
            String pathStr = V6Value.argAt(args, 0).toString();
            V6Array result = new V6Array();
            try (DirectoryStream<Path> stream = Files.newDirectoryStream(Paths.get(pathStr))) {
              List<String> names = new ArrayList<>();
              for (Path entry : stream)
                names.add(entry.getFileName().toString());
              java.util.Collections.sort(names);
              for (String n : names)
                result.push(str(n));
            } catch (IOException e) {
              throw ioError("ENOENT: no such directory", pathStr, e);
            }
            return objValue(result);
          }));

    o.set("statSync", fn((thisArg, args) -> {
            String pathStr = V6Value.argAt(args, 0).toString();
            try {
              return objValue(statsObject(Paths.get(pathStr)));
            } catch (IOException e) {
              throw ioError("ENOENT: no such file or directory, stat", pathStr, e);
            }
          }));
    o.set("lstatSync", o.get("statSync"));

    o.set("unlinkSync", fn((thisArg, args) -> {
            String pathStr = V6Value.argAt(args, 0).toString();
            try {
              Files.delete(Paths.get(pathStr));
            } catch (IOException e) {
              throw ioError("ENOENT: no such file", pathStr, e);
            }
            return UNDEF;
          }));

    o.set("rmdirSync", fn((thisArg, args) -> {
            String pathStr = V6Value.argAt(args, 0).toString();
            boolean recursive = false;
            if (args.length > 1 && args[1].tag() == V6Value.TAG_OBJ) {
              V6Value r = ((V6Object)args[1].ref()).get("recursive");
              recursive = r.truthy();
            }
            try {
              if (recursive)
                deleteRecursive(Paths.get(pathStr));
              else
                Files.delete(Paths.get(pathStr));
            } catch (IOException e) {
              throw ioError("ENOENT: could not remove directory", pathStr, e);
            }
            return UNDEF;
          }));

    o.set("rmSync", fn((thisArg, args) -> {
            String pathStr = V6Value.argAt(args, 0).toString();
            boolean recursive = false;
            boolean force = false;
            if (args.length > 1 && args[1].tag() == V6Value.TAG_OBJ) {
              V6Object opts = (V6Object)args[1].ref();
              recursive = opts.get("recursive").truthy();
              force = opts.get("force").truthy();
            }
            try {
              Path p = Paths.get(pathStr);
              if (recursive && Files.exists(p))
                deleteRecursive(p);
              else if (!force || Files.exists(p))
                Files.delete(p);
            } catch (IOException e) {
              if (!force)
                throw ioError("ENOENT: could not remove", pathStr, e);
            }
            return UNDEF;
          }));

    o.set("renameSync", fn((thisArg, args) -> {
            String from = V6Value.argAt(args, 0).toString();
            String to = V6Value.argAt(args, 1).toString();
            try {
              Files.move(Paths.get(from), Paths.get(to),
                        java.nio.file.StandardCopyOption.REPLACE_EXISTING);
            } catch (IOException e) {
              throw ioError("ENOENT: could not rename", from, e);
            }
            return UNDEF;
          }));

    o.set("copyFileSync", fn((thisArg, args) -> {
            String from = V6Value.argAt(args, 0).toString();
            String to = V6Value.argAt(args, 1).toString();
            try {
              Files.copy(Paths.get(from), Paths.get(to),
                        java.nio.file.StandardCopyOption.REPLACE_EXISTING);
            } catch (IOException e) {
              throw ioError("ENOENT: could not copy", from, e);
            }
            return UNDEF;
          }));

    o.set("symlinkSync", fn((thisArg, args) -> {
            String target = V6Value.argAt(args, 0).toString();
            String linkPath = V6Value.argAt(args, 1).toString();
            try {
              Files.createSymbolicLink(Paths.get(linkPath), Paths.get(target));
            } catch (IOException e) {
              throw ioError("EEXIST: could not create symlink", linkPath, e);
            }
            return UNDEF;
          }));

    o.set("readlinkSync", fn((thisArg, args) -> {
            String pathStr = V6Value.argAt(args, 0).toString();
            try {
              return str(Files.readSymbolicLink(Paths.get(pathStr)).toString());
            } catch (IOException e) {
              throw ioError("ENOENT: no such symlink", pathStr, e);
            }
          }));

    o.set("constants", objValue(buildConstants()));

    wireAsyncVariants(o);
    o.set("promises", objValue(buildPromises(o)));
    wireWatch(o);

    return o;
  }

  private static V6Object buildConstants() {
    V6Object c = new V6Object();
    c.set("F_OK", num(0));
    c.set("R_OK", num(4));
    c.set("W_OK", num(2));
    c.set("X_OK", num(1));
    c.set("O_RDONLY", num(0));
    c.set("O_WRONLY", num(1));
    c.set("O_RDWR", num(2));
    c.set("O_CREAT", num(64));
    c.set("O_EXCL", num(128));
    c.set("O_TRUNC", num(512));
    c.set("O_APPEND", num(1024));
    return c;
  }

  private static V6Value promisified(V6Object o, String syncName) {
    return fn((thisArg, args) -> {
      V6Promise p = new V6Promise();
      try {
        V6Value result = o.get(syncName).asCallable().call(thisArg, args);
        p.resolve(result);
      } catch (V6Throw e) {
        p.reject(e.value);
      }
      return objValue(p);
    });
  }

  private static V6Object buildPromises(V6Object o) {
    V6Object p = new V6Object();
    p.set("readFile", promisified(o, "readFileSync"));
    p.set("writeFile", promisified(o, "writeFileSync"));
    p.set("appendFile", promisified(o, "appendFileSync"));
    p.set("mkdir", promisified(o, "mkdirSync"));
    p.set("readdir", promisified(o, "readdirSync"));
    p.set("stat", promisified(o, "statSync"));
    p.set("lstat", promisified(o, "statSync"));
    p.set("unlink", promisified(o, "unlinkSync"));
    p.set("rmdir", promisified(o, "rmdirSync"));
    p.set("rm", promisified(o, "rmSync"));
    p.set("rename", promisified(o, "renameSync"));
    p.set("copyFile", promisified(o, "copyFileSync"));
    p.set("symlink", promisified(o, "symlinkSync"));
    p.set("readlink", promisified(o, "readlinkSync"));
    return p;
  }

  private static final Map<String, boolean[]> watchFileStops =
      new java.util.concurrent.ConcurrentHashMap<>();

  private static void wireWatch(V6Object o) {
    o.set("watch", fn((thisArg, args) -> {
            String pathStr = V6Value.argAt(args, 0).toString();
            V6Callable listener = null;
            for (int i = 1; i < args.length; i++)
              if (args[i].tag() == V6Value.TAG_FUNC) {
                listener = args[i].asCallable();
                break;
              }
            final V6Callable cb = listener;
            Path target = Paths.get(pathStr);
            Path dir = Files.isDirectory(target) ? target : target.getParent();
            if (dir == null)
              dir = Paths.get(".");
            final String watchName =
                Files.isDirectory(target) ? null : target.getFileName().toString();

            V6EventEmitterObject watcher = new V6EventEmitterObject();
            watcher.setProto(V6EventEmitterConstructor.PROTOTYPE);
            boolean[] closed = {false};

            try {
              java.nio.file.WatchService ws = dir.getFileSystem().newWatchService();
              dir.register(ws, java.nio.file.StandardWatchEventKinds.ENTRY_CREATE,
                           java.nio.file.StandardWatchEventKinds.ENTRY_MODIFY,
                           java.nio.file.StandardWatchEventKinds.ENTRY_DELETE);
              V6EventLoop.ref();
              Thread th = new Thread(() -> {
                try {
                  while (!closed[0]) {
                    java.nio.file.WatchKey key;
                    try {
                      key = ws.poll(200, java.util.concurrent.TimeUnit.MILLISECONDS);
                    } catch (InterruptedException ie) {
                      break;
                    }
                    if (key == null)
                      continue;
                    for (java.nio.file.WatchEvent<?> ev : key.pollEvents()) {
                      Object ctx = ev.context();
                      String changedName = ctx != null ? ctx.toString() : "";
                      if (watchName != null && !changedName.equals(watchName))
                        continue;
                      String eventType =
                          ev.kind() == java.nio.file.StandardWatchEventKinds.ENTRY_MODIFY
                              ? "change"
                              : "rename";
                      final String fName = changedName;
                      final String eType = eventType;
                      V6EventLoop.postExternal(() -> {
                        if (cb != null)
                          cb.call(UNDEF, new V6Value[] {str(eType), str(fName)});
                        watcher.get("emit").asCallable().call(
                            objValue(watcher),
                            new V6Value[] {str("change"), str(eType), str(fName)});
                      });
                    }
                    if (!key.reset())
                      break;
                  }
                } finally {
                  try {
                    ws.close();
                  } catch (IOException ignored) {
                  }
                  V6EventLoop.unref();
                }
              });
              th.setDaemon(true);
              th.start();
              watcher.set("close", fn((t, a) -> {
                            closed[0] = true;
                            return UNDEF;
                          }));
            } catch (IOException e) {
              throw ioError("ENOENT: could not watch", pathStr, e);
            }

            return objValue(watcher);
          }));

    o.set("watchFile", fn((thisArg, args) -> {
            String pathStr = V6Value.argAt(args, 0).toString();
            V6Callable listener = null;
            long interval = 1000;
            for (int i = 1; i < args.length; i++) {
              if (args[i].tag() == V6Value.TAG_FUNC) {
                listener = args[i].asCallable();
              } else if (args[i].tag() == V6Value.TAG_OBJ) {
                V6Value iv = ((V6Object)args[i].ref()).get("interval");
                if (iv.tag() == V6Value.TAG_NUM)
                  interval = (long)iv.toNumber();
              }
            }
            final V6Callable cb = listener;
            final long pollMs = interval;
            Path p = Paths.get(pathStr);
            boolean[] stopped = {false};
            V6EventLoop.ref();
            Thread th = new Thread(() -> {
              V6Object prev = null;
              try {
                while (!stopped[0]) {
                  V6Object cur;
                  try {
                    cur = statsObject(p);
                  } catch (IOException e) {
                    cur = null;
                  }
                  final V6Object curF = cur;
                  final V6Object prevF = prev;
                  if (cb != null && prevF != null)
                    V6EventLoop.postExternal(
                        ()
                            -> cb.call(UNDEF, new V6Value[] {
                                 curF != null ? objValue(curF) : UNDEF, objValue(prevF)
                               }));
                  prev = cur;
                  Thread.sleep(pollMs);
                }
              } catch (InterruptedException ignored) {
              } finally {
                V6EventLoop.unref();
              }
            });
            th.setDaemon(true);
            th.start();
            watchFileStops.put(pathStr, stopped);
            return UNDEF;
          }));

    o.set("unwatchFile", fn((thisArg, args) -> {
            String pathStr = V6Value.argAt(args, 0).toString();
            boolean[] stopped = watchFileStops.remove(pathStr);
            if (stopped != null)
              stopped[0] = true;
            return UNDEF;
          }));
  }

  private static byte[] toBytesForWrite(V6Value data, String encoding) {
    if (data.tag() == V6Value.TAG_OBJ && data.ref() instanceof V6Buffer)
      return ((V6Buffer)data.ref()).toBytes();
    return V6BufferConstructor.decodeString(data.toString(),
                                            encoding != null ? encoding : "utf8");
  }

  private static void deleteRecursive(Path root) throws IOException {
    if (!Files.exists(root))
      return;
    List<Path> paths = new ArrayList<>();
    try (java.util.stream.Stream<Path> walk = Files.walk(root)) {
      walk.forEach(paths::add);
    }
    paths.sort(java.util.Comparator.reverseOrder());
    for (Path p : paths)
      Files.delete(p);
  }

  private static void wireAsyncVariants(V6Object o) {
    o.set("readFile", fn((thisArg, args) -> {
            V6Callable cb = args[args.length - 1].asCallable();
            try {
              V6Value result =
                  o.get("readFileSync").asCallable().call(thisArg,
                                                          java.util.Arrays.copyOf(
                                                              args, args.length - 1));
              deferCallback(cb, NUL, result);
            } catch (V6Throw e) {
              deferCallback(cb, e.value, UNDEF);
            }
            return UNDEF;
          }));
    o.set("writeFile", asyncWrap(o, "writeFileSync"));
    o.set("appendFile", asyncWrap(o, "appendFileSync"));
    o.set("mkdir", asyncWrap(o, "mkdirSync"));
    o.set("readdir", asyncWrap(o, "readdirSync"));
    o.set("stat", asyncWrap(o, "statSync"));
    o.set("unlink", asyncWrap(o, "unlinkSync"));
    o.set("rmdir", asyncWrap(o, "rmdirSync"));
    o.set("rm", asyncWrap(o, "rmSync"));
    o.set("rename", asyncWrap(o, "renameSync"));
    o.set("copyFile", asyncWrap(o, "copyFileSync"));
    o.set("symlink", asyncWrap(o, "symlinkSync"));
    o.set("readlink", asyncWrap(o, "readlinkSync"));
  }

  private static V6Value asyncWrap(V6Object o, String syncName) {
    return fn((thisArg, args) -> {
      V6Callable cb = args[args.length - 1].asCallable();
      V6Value[] syncArgs = java.util.Arrays.copyOf(args, args.length - 1);
      try {
        V6Value result = o.get(syncName).asCallable().call(thisArg, syncArgs);
        deferCallback(cb, NUL, result);
      } catch (V6Throw e) {
        deferCallback(cb, e.value, UNDEF);
      }
      return UNDEF;
    });
  }
}
