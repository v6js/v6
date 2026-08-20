import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;

public final class V6WasmGlobal {
  private static final V6DaemonClassLoader LOADER =
      new V6DaemonClassLoader(V6WasmGlobal.class.getClassLoader());
  private static int moduleCounter = 0;

  public static V6Value instantiateBytesSync(byte[] wasmBytes) {
    V6WasmModuleObject module = compileModule(wasmBytes);
    V6Object instance = instantiateModule(module, null);
    return instance.get("exports");
  }

  public static V6Object build() {
    V6Object o = new V6Object();
    o.set("compile", fn(V6WasmGlobal::compile));
    o.set("instantiate", fn(V6WasmGlobal::instantiate));
    o.set("validate", fn(V6WasmGlobal::validate));
    return o;
  }

  private static V6Value fn(V6Callable c) {
    return new V6Value(V6Value.TAG_FUNC, 0, c);
  }

  private static V6Value obj(Object o) {
    return new V6Value(V6Value.TAG_OBJ, 0, o);
  }

  private static V6Value str(String s) {
    return new V6Value(V6Value.TAG_STR, 0, s);
  }

  private static byte[] toBytes(V6Value v) {
    if (v != null && v.tag() == V6Value.TAG_OBJ) {
      Object ref = v.ref();
      if (ref instanceof V6Uint8ArrayObject u8)
        return u8.toBytes();
      if (ref instanceof V6ArrayBufferObject ab)
        return ab.data;
      if (ref instanceof V6Buffer buf)
        return buf.toBytes();
    }
    throw new RuntimeException("TypeError: WebAssembly expects a "
                               + "BufferSource (Uint8Array or ArrayBuffer)");
  }

  static V6WasmModuleObject compileModule(byte[] wasmBytes) {
    String className = "V6WasmMod" + (moduleCounter++);
    byte[] classBytes = V6WasmCompiler.compile(wasmBytes, className);
    String exportManifest = V6WasmCompiler.describeExports(wasmBytes);
    String importManifest = V6WasmCompiler.describeImports(wasmBytes);
    return new V6WasmModuleObject(className, classBytes, exportManifest,
                                  importManifest);
  }

  private static Class<?>[] paramTypesOf(String desc) {
    List<Class<?>> types = new ArrayList<>();
    int i = 1;
    while (desc.charAt(i) != ')') {
      switch (desc.charAt(i)) {
      case 'I':
        types.add(int.class);
        break;
      case 'J':
        types.add(long.class);
        break;
      case 'F':
        types.add(float.class);
        break;
      case 'D':
        types.add(double.class);
        break;
      default:
        throw new RuntimeException("unsupported wasm export param type");
      }
      i++;
    }
    return types.toArray(new Class<?>[0]);
  }

  private static V6Callable makeExportFn(Method method) {
    Class<?>[] paramTypes = method.getParameterTypes();
    return (thisArg, args) -> {
      Object[] javaArgs = new Object[paramTypes.length];
      for (int i = 0; i < paramTypes.length; i++) {
        double d = i < args.length ? args[i].toNumber() : 0;
        Class<?> pt = paramTypes[i];
        if (pt == int.class)
          javaArgs[i] = (int)d;
        else if (pt == long.class)
          javaArgs[i] = (long)d;
        else if (pt == float.class)
          javaArgs[i] = (float)d;
        else
          javaArgs[i] = d;
      }
      try {
        Object result = method.invoke(null, javaArgs);
        if (result instanceof Integer r)
          return V6Value.num(r);
        if (result instanceof Long r)
          return V6Value.num(r);
        if (result instanceof Float r)
          return V6Value.num(r);
        if (result instanceof Double r)
          return V6Value.num(r);
        return V6Value.UNDEF;
      } catch (InvocationTargetException e) {
        if (e.getCause() instanceof RuntimeException re)
          throw re;
        throw new RuntimeException(e.getCause());
      } catch (ReflectiveOperationException e) {
        throw new RuntimeException(e);
      }
    };
  }

  private static void resolveImports(Class<?> cls, V6WasmModuleObject module,
                                     V6Value importObject) {
    for (String line : module.importManifest.split("\n")) {
      if (line.isEmpty())
        continue;
      String[] parts = line.split("\t");
      if (parts.length != 3)
        continue;
      String moduleName = parts[0];
      String fieldName = parts[1];
      String funcIdx = parts[2];
      if (importObject == null || importObject.tag() != V6Value.TAG_OBJ ||
          !(importObject.ref() instanceof V6Object impObj)) {
        throw new RuntimeException("LinkError: module requires imports but "
                                   + "no importObject was provided");
      }
      V6Value modVal = impObj.get(moduleName);
      if (modVal == null || modVal.tag() != V6Value.TAG_OBJ ||
          !(modVal.ref() instanceof V6Object modObj)) {
        throw new RuntimeException("LinkError: import module '" + moduleName +
                                   "' not found");
      }
      V6Value fieldVal = modObj.get(fieldName);
      if (fieldVal == null || fieldVal.tag() == V6Value.TAG_UNDEF) {
        throw new RuntimeException("LinkError: import '" + moduleName + "." +
                                   fieldName + "' not found");
      }
      try {
        Method setter = cls.getMethod("wasmSetImport" + funcIdx, V6Value.class);
        setter.invoke(null, fieldVal);
      } catch (ReflectiveOperationException e) {
        throw new RuntimeException("LinkError: " + e.getMessage());
      }
    }
  }

  static V6WasmInstanceObject instantiateModule(V6WasmModuleObject module,
                                                V6Value importObject) {
    Class<?> cls = LOADER.defineFromBytes(module.className, module.classBytes);
    resolveImports(cls, module, importObject);
    V6Object exports = new V6Object();
    for (String line : module.exportManifest.split("\n")) {
      if (line.isEmpty())
        continue;
      String[] parts = line.split("\t");
      if (parts.length != 3)
        continue;
      String name = parts[0];
      String funcIdx = parts[1];
      String desc = parts[2];
      try {
        Method method = cls.getMethod("wasmFunc" + funcIdx, paramTypesOf(desc));
        exports.set(name, fn(makeExportFn(method)));
      } catch (NoSuchMethodException e) {
        throw new RuntimeException("LinkError: " + e.getMessage());
      }
    }
    V6WasmInstanceObject instance = new V6WasmInstanceObject(cls);
    instance.set("exports", obj(exports));
    return instance;
  }

  private static V6Value compile(V6Value thisArg, V6Value[] args) {
    V6Promise p = new V6Promise();
    try {
      V6WasmModuleObject module =
          compileModule(toBytes(args.length > 0 ? args[0] : null));
      p.resolve(obj(module));
    } catch (Throwable t) {
      p.reject(str("CompileError: " + t.getMessage()));
    }
    return obj(p);
  }

  private static V6Value instantiate(V6Value thisArg, V6Value[] args) {
    V6Promise p = new V6Promise();
    try {
      V6Value first = args.length > 0 ? args[0] : null;
      V6Value importObject = args.length > 1 ? args[1] : null;
      V6WasmModuleObject module;
      boolean wasModuleArg = false;
      if (first != null && first.tag() == V6Value.TAG_OBJ &&
          first.ref() instanceof V6WasmModuleObject m) {
        module = m;
        wasModuleArg = true;
      } else {
        module = compileModule(toBytes(first));
      }
      V6Object instance = instantiateModule(module, importObject);
      if (wasModuleArg) {
        p.resolve(obj(instance));
      } else {
        V6Object result = new V6Object();
        result.set("module", obj(module));
        result.set("instance", obj(instance));
        p.resolve(obj(result));
      }
    } catch (Throwable t) {
      p.reject(str("LinkError: " + t.getMessage()));
    }
    return obj(p);
  }

  private static V6Value validate(V6Value thisArg, V6Value[] args) {
    try {
      V6WasmCompiler.describeExports(toBytes(args.length > 0 ? args[0] : null));
      return V6Value.TRUE;
    } catch (Throwable t) {
      return V6Value.FALSE;
    }
  }
}
