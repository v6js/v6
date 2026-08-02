import java.io.IOException;
import java.nio.file.DirectoryStream;
import java.nio.file.Files;
import java.nio.file.LinkOption;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.attribute.BasicFileAttributes;
import java.util.ArrayList;
import java.util.List;

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

    wireAsyncVariants(o);

    return o;
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
