import java.lang.reflect.Array;
import java.lang.reflect.Constructor;
import java.lang.reflect.Executable;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.lang.reflect.Proxy;
import java.math.BigInteger;
import java.util.ArrayList;
import java.util.List;

public final class V6JavaMarshal {
  private V6JavaMarshal() {
  }

  private static final V6Value UNDEF = new V6Value(V6Value.TAG_UNDEF, 0, null);
  private static final V6Value NUL = new V6Value(V6Value.TAG_NULL, 0, null);

  private static final int SCORE_EXACT = 0;
  private static final int SCORE_GOOD = 10;
  private static final int SCORE_COERCE = 50;
  private static final int SCORE_OBJECT_FALLBACK = 100;
  private static final int SCORE_REJECT = Integer.MAX_VALUE;

  private static V6Value str(String s) {
    return new V6Value(V6Value.TAG_STR, 0, s);
  }

  private static V6Value fn(V6Callable c) {
    return new V6Value(V6Value.TAG_FUNC, 0, c);
  }

  private static V6Value objValue(V6Object o) {
    return new V6Value(V6Value.TAG_OBJ, 0, o);
  }

  static V6Value toJs(Object o) {
    if (o == null)
      return NUL;
    if (o instanceof String || o instanceof CharSequence)
      return str(o.toString());
    if (o instanceof Character)
      return str(o.toString());
    if (o instanceof Boolean)
      return new V6Value(V6Value.TAG_BOOL, ((Boolean)o) ? 1 : 0, null);
    if (o instanceof BigInteger)
      return new V6Value(V6Value.TAG_BIGINT, 0, o);
    if (o instanceof Number)
      return new V6Value(V6Value.TAG_NUM, ((Number)o).doubleValue(), null);
    if (o instanceof Class)
      return V6JavaClassObject.wrap((Class<?>)o);
    if (o.getClass().isArray()) {
      V6Array arr = new V6Array();
      int n = Array.getLength(o);
      for (int i = 0; i < n; i++)
        arr.push(toJs(Array.get(o, i)));
      return objValue(arr);
    }
    return objValue(new V6JavaInstanceObject(o));
  }

  private static boolean isNumericType(Class<?> t) {
    return t == int.class || t == Integer.class || t == long.class ||
        t == Long.class || t == double.class || t == Double.class ||
        t == float.class || t == Float.class || t == short.class ||
        t == Short.class || t == byte.class || t == Byte.class;
  }

  private static int scoreParam(Class<?> type, V6Value arg) {
    if (arg.isNullish()) {
      if (type.isPrimitive())
        return SCORE_REJECT;
      return SCORE_GOOD;
    }
    if (type == boolean.class || type == Boolean.class)
      return arg.tag() == V6Value.TAG_BOOL ? SCORE_EXACT : SCORE_REJECT;
    if (isNumericType(type)) {
      if (arg.tag() == V6Value.TAG_NUM)
        return SCORE_EXACT;
      if (arg.tag() == V6Value.TAG_BIGINT)
        return SCORE_GOOD;
      if (arg.tag() == V6Value.TAG_STR) {
        try {
          Double.parseDouble(arg.toString());
          return SCORE_COERCE;
        } catch (NumberFormatException e) {
          return SCORE_REJECT;
        }
      }
      return SCORE_REJECT;
    }
    if (type == BigInteger.class)
      return arg.tag() == V6Value.TAG_BIGINT ? SCORE_EXACT
      : arg.tag() == V6Value.TAG_NUM         ? SCORE_COERCE
                                             : SCORE_REJECT;
    if (type == char.class || type == Character.class)
      return arg.tag() == V6Value.TAG_STR && arg.toString().length() >= 1
          ? SCORE_GOOD
          : SCORE_REJECT;
    if (type == String.class || type == CharSequence.class)
      return arg.tag() == V6Value.TAG_STR ? SCORE_EXACT : SCORE_COERCE;
    if (type.isArray())
      return arg.tag() == V6Value.TAG_OBJ && arg.ref() instanceof V6Array
          ? SCORE_GOOD
          : SCORE_REJECT;
    if (type.isInterface()) {
      if (arg.tag() == V6Value.TAG_FUNC)
        return SCORE_GOOD;
      if (arg.tag() == V6Value.TAG_OBJ && !(arg.ref() instanceof V6Array) &&
          !(arg.ref() instanceof V6JavaInstanceObject) &&
          !(arg.ref() instanceof V6JavaClassObject))
        return SCORE_GOOD;
    }
    if (arg.tag() == V6Value.TAG_OBJ && arg.ref() instanceof
                                            V6JavaInstanceObject) {
      Object inst = ((V6JavaInstanceObject)arg.ref()).instance;
      if (type.isInstance(inst))
        return SCORE_EXACT;
      return type == Object.class ? SCORE_OBJECT_FALLBACK : SCORE_REJECT;
    }
    if (arg.tag() == V6Value.TAG_OBJ && arg.ref() instanceof
                                            V6JavaClassObject) {
      Class<?> wrapped = ((V6JavaClassObject)arg.ref()).clazz;
      if (type == Class.class)
        return SCORE_EXACT;
      return type == Object.class ? SCORE_OBJECT_FALLBACK : SCORE_REJECT;
    }
    if (type == Object.class)
      return SCORE_OBJECT_FALLBACK;
    return SCORE_REJECT;
  }

  static Object toJava(V6Value v, Class<?> type) {
    if (type == void.class || type == Void.class)
      return null;
    if (v.isNullish())
      return null;
    if (type == boolean.class || type == Boolean.class)
      return v.truthy();
    if (type == int.class || type == Integer.class)
      return (int)v.toNumber();
    if (type == long.class || type == Long.class)
      return (long)v.toNumber();
    if (type == short.class || type == Short.class)
      return (short)v.toNumber();
    if (type == byte.class || type == Byte.class)
      return (byte)v.toNumber();
    if (type == double.class || type == Double.class)
      return v.toNumber();
    if (type == float.class || type == Float.class)
      return (float)v.toNumber();
    if (type == BigInteger.class)
      return v.tag() == V6Value.TAG_BIGINT
          ? v.asBigInt()
          : BigInteger.valueOf((long)v.toNumber());
    if (type == char.class || type == Character.class) {
      String s = v.toString();
      return s.isEmpty() ? '\0' : s.charAt(0);
    }
    if (type == String.class || type == CharSequence.class)
      return v.toString();
    if (type.isArray()) {
      Class<?> component = type.getComponentType();
      if (v.tag() == V6Value.TAG_OBJ && v.ref() instanceof V6Array) {
        V6Array arr = (V6Array)v.ref();
        int n = (int)arr.get("length").num();
        Object out = Array.newInstance(component, n);
        for (int i = 0; i < n; i++)
          Array.set(out, i, toJava(arr.get(Integer.toString(i)), component));
        return out;
      }
      return Array.newInstance(component, 0);
    }
    if (v.tag() == V6Value.TAG_OBJ && v.ref() instanceof V6JavaInstanceObject)
      return ((V6JavaInstanceObject)v.ref()).instance;
    if (v.tag() == V6Value.TAG_OBJ && v.ref() instanceof V6JavaClassObject)
      return type == Class.class ? ((V6JavaClassObject)v.ref()).clazz : v;
    if (type.isInterface() &&
        (v.tag() == V6Value.TAG_FUNC || v.tag() == V6Value.TAG_OBJ))
      return makeProxy(v, type);
    return v.toString();
  }

  private static Object makeProxy(V6Value jsTarget, Class<?> iface) {
    return Proxy.newProxyInstance(iface.getClassLoader(),
                                  new Class<?>[] {iface},
                                  new V6JavaProxyHandler(jsTarget));
  }

  private static V6JavaMatch tryMatch(Executable exec, V6Value[] args) {
    Class<?>[] params = exec.getParameterTypes();
    boolean varargs = exec.isVarArgs();
    if (!varargs && params.length != args.length)
      return null;
    if (varargs && args.length < params.length - 1)
      return null;

    if (!varargs || (args.length == params.length &&
                     scoreParam(params[params.length - 1],
                                args[params.length - 1]) != SCORE_REJECT)) {
      int total = 0;
      for (int i = 0; i < params.length; i++) {
        int s = scoreParam(params[i], args[i]);
        if (s == SCORE_REJECT) {
          total = SCORE_REJECT;
          break;
        }
        total += s;
      }
      if (total != SCORE_REJECT) {
        Object[] marshalled = new Object[params.length];
        for (int i = 0; i < params.length; i++)
          marshalled[i] = toJava(args[i], params[i]);
        V6JavaMatch m = new V6JavaMatch();
        m.exec = exec;
        m.score = total;
        m.args = marshalled;
        return m;
      }
    }

    if (varargs) {
      Class<?> component = params[params.length - 1].getComponentType();
      int fixed = params.length - 1;
      int total = 0;
      for (int i = 0; i < fixed; i++) {
        int s = scoreParam(params[i], args[i]);
        if (s == SCORE_REJECT)
          return null;
        total += s;
      }
      for (int i = fixed; i < args.length; i++) {
        int s = scoreParam(component, args[i]);
        if (s == SCORE_REJECT)
          return null;
        total += s;
      }
      Object[] marshalled = new Object[params.length];
      for (int i = 0; i < fixed; i++)
        marshalled[i] = toJava(args[i], params[i]);
      Object varArray = Array.newInstance(component, args.length - fixed);
      for (int i = fixed; i < args.length; i++)
        Array.set(varArray, i - fixed, toJava(args[i], component));
      marshalled[fixed] = varArray;
      V6JavaMatch m = new V6JavaMatch();
      m.exec = exec;
      m.score = total + 5;
      m.args = marshalled;
      return m;
    }
    return null;
  }

  private static V6JavaMatch pickBest(Executable[] candidates, V6Value[] args,
                                      String label) {
    V6JavaMatch best = null;
    for (Executable e : candidates) {
      V6JavaMatch m = tryMatch(e, args);
      if (m != null && (best == null || m.score < best.score))
        best = m;
    }
    if (best == null)
      throw new V6Throw(str("TypeError: no matching overload for " + label +
                            "(" + args.length + " args)"));
    return best;
  }

  private static RuntimeException wrapInvokeError(Throwable t, String label) {
    Throwable cause =
        t instanceof InvocationTargetException && t.getCause() != null
            ? t.getCause()
            : t;
    return new V6Throw(str(cause.getClass().getSimpleName() + ": " +
                           cause.getMessage() + " (in " + label + ")"));
  }

  static V6Value construct(Class<?> clazz, V6Value[] args) {
    Constructor<?>[] ctors = clazz.getConstructors();
    V6JavaMatch m = pickBest(ctors, args, clazz.getName());
    try {
      Object instance = ((Constructor<?>)m.exec).newInstance(m.args);
      return toJs(instance);
    } catch (Exception e) {
      throw wrapInvokeError(e, "new " + clazz.getName());
    }
  }

  private static Method[] methodsNamed(Class<?> cls, String name) {
    List<Method> out = new ArrayList<>();
    for (Method meth : cls.getMethods())
      if (meth.getName().equals(name))
        out.add(publicEquivalent(meth));
    return out.toArray(new Method[0]);
  }

  private static Method publicEquivalent(Method m) {
    if (Modifier.isPublic(m.getDeclaringClass().getModifiers()))
      return m;
    Method found = findInHierarchy(m.getDeclaringClass(), m.getName(),
                                   m.getParameterTypes());
    return found != null ? found : m;
  }

  private static Method findInHierarchy(Class<?> cls, String name,
                                        Class<?>[] paramTypes) {
    if (cls == null)
      return null;
    if (Modifier.isPublic(cls.getModifiers())) {
      try {
        return cls.getMethod(name, paramTypes);
      } catch (NoSuchMethodException ignored) {
      }
    }
    for (Class<?> iface : cls.getInterfaces()) {
      Method found = findInHierarchy(iface, name, paramTypes);
      if (found != null)
        return found;
    }
    return findInHierarchy(cls.getSuperclass(), name, paramTypes);
  }

  private static Object invokeBestOverload(Object target, Method[] methods,
                                           V6Value[] args, String label) {
    V6JavaMatch m = pickBest(methods, args, label);
    try {
      ((Method)m.exec).setAccessible(true);
      Object result = ((Method)m.exec).invoke(target, m.args);
      return result;
    } catch (Exception e) {
      throw wrapInvokeError(e, label);
    }
  }

  static V6Value resolveInstanceMember(Object instance, String name) {
    Class<?> cls = instance.getClass();
    Method[] methods = methodsNamed(cls, name);
    if (methods.length > 0)
      return fn(
          (thisArg, args)
              -> toJs(invokeBestOverload(instance, methods, args,
                                         cls.getSimpleName() + "." + name)));
    Field f = publicFieldNamed(cls, name);
    if (f != null) {
      try {
        f.setAccessible(true);
        return toJs(f.get(instance));
      } catch (IllegalAccessException e) {
        throw new V6Throw(str("Error: cannot read field " + name));
      }
    }
    return UNDEF;
  }

  static boolean trySetInstanceField(Object instance, String name,
                                     V6Value value) {
    Field f = publicFieldNamed(instance.getClass(), name);
    if (f == null || Modifier.isFinal(f.getModifiers()))
      return false;
    try {
      f.setAccessible(true);
      f.set(instance, toJava(value, f.getType()));
      return true;
    } catch (IllegalAccessException e) {
      return false;
    }
  }

  static V6Value resolveStaticMember(Class<?> cls, String name) {
    List<Method> statics = new ArrayList<>();
    for (Method meth : cls.getMethods())
      if (meth.getName().equals(name) && Modifier.isStatic(meth.getModifiers()))
        statics.add(publicEquivalent(meth));
    if (!statics.isEmpty()) {
      Method[] arr = statics.toArray(new Method[0]);
      return fn((thisArg, args)
                    -> toJs(invokeBestOverload(
                        null, arr, args, cls.getSimpleName() + "." + name)));
    }
    Field f = publicFieldNamed(cls, name);
    if (f != null && Modifier.isStatic(f.getModifiers())) {
      try {
        f.setAccessible(true);
        return toJs(f.get(null));
      } catch (IllegalAccessException e) {
        throw new V6Throw(str("Error: cannot read static field " + name));
      }
    }
    for (Class<?> nested : cls.getClasses())
      if (nested.getSimpleName().equals(name))
        return V6JavaClassObject.wrap(nested);
    return UNDEF;
  }

  static boolean trySetStaticField(Class<?> cls, String name, V6Value value) {
    Field f = publicFieldNamed(cls, name);
    if (f == null || !Modifier.isStatic(f.getModifiers()) ||
        Modifier.isFinal(f.getModifiers()))
      return false;
    try {
      f.setAccessible(true);
      f.set(null, toJava(value, f.getType()));
      return true;
    } catch (IllegalAccessException e) {
      return false;
    }
  }

  private static Field publicFieldNamed(Class<?> cls, String name) {
    try {
      return cls.getField(name);
    } catch (NoSuchFieldException e) {
      return null;
    }
  }

  static V6Value defaultReturnFor(Class<?> returnType) {
    if (returnType == void.class || returnType == Void.class)
      return UNDEF;
    if (returnType == boolean.class || returnType == Boolean.class)
      return new V6Value(V6Value.TAG_BOOL, 0, null);
    if (isNumericType(returnType))
      return new V6Value(V6Value.TAG_NUM, 0, null);
    return NUL;
  }
}
