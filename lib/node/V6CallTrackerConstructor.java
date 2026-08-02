public final class V6CallTrackerConstructor
    extends V6Object implements V6NativeConstructor {
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

  @Override
  public V6Value construct(V6Value[] args) {
    V6Object self = new V6Object();
    java.util.List<Integer> exactCounts = new java.util.ArrayList<>();
    java.util.List<int[]> actualCounts = new java.util.ArrayList<>();

    self.set("calls", fn((t, a) -> {
               V6Value first = V6Value.argAt(a, 0);
               V6Callable target;
               int exact;
               if (first.tag() == V6Value.TAG_FUNC) {
                 target = first.asCallable();
                 exact = a.length > 1 ? (int)a[1].toNumber() : 1;
               } else if (first.tag() == V6Value.TAG_NUM) {
                 target = (thisArg2, args2) -> UNDEF;
                 exact = (int)first.toNumber();
               } else {
                 target = (thisArg2, args2) -> UNDEF;
                 exact = 1;
               }
               int idx = exactCounts.size();
               exactCounts.add(exact);
               actualCounts.add(new int[1]);
               V6Callable wrapped = (thisArg2, args2) -> {
                 actualCounts.get(idx)[0]++;
                 return target.call(thisArg2, args2);
               };
               return fn(wrapped);
             }));

    self.set(
        "report", fn((t, a) -> {
          V6Array result = new V6Array();
          for (int i = 0; i < exactCounts.size(); i++) {
            int expected = exactCounts.get(i);
            int actual = actualCounts.get(i)[0];
            if (actual != expected) {
              V6Object rec = new V6Object();
              rec.set("message", str("Expected the function to be executed " +
                                     expected + " time(s) but was executed " +
                                     actual + " time(s)."));
              rec.set("actual", new V6Value(V6Value.TAG_NUM, actual, null));
              rec.set("expected", new V6Value(V6Value.TAG_NUM, expected, null));
              rec.set("operator", str("calls"));
              result.push(objValue(rec));
            }
          }
          return objValue(result);
        }));

    self.set(
        "verify", fn((t, a) -> {
          V6Value reportVal =
              self.get("report").asCallable().call(t, new V6Value[0]);
          V6Array report = (V6Array)reportVal.ref();
          if (report.elemCount > 0) {
            StringBuilder sb = new StringBuilder(
                "Number of calls to functions do not match expectations:\n");
            for (int i = 0; i < report.elemCount; i++) {
              V6Object rec = (V6Object)report.elements[i].ref();
              sb.append(rec.get("message").toString()).append("\n");
            }
            throw new V6Throw(str(sb.toString()));
          }
          return UNDEF;
        }));

    self.set("reset", fn((t, a) -> {
               for (int[] c : actualCounts)
                 c[0] = 0;
               return UNDEF;
             }));

    return objValue(self);
  }
}
