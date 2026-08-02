import java.util.ArrayDeque;

public final class V6Readline {
  private V6Readline() {
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

  public static V6Object build() {
    V6Object o = new V6Object();

    o.set("createInterface", fn((thisArg, args) -> {
            V6Value first = V6Value.argAt(args, 0);
            V6Value inputVal;
            V6Value outputVal = UNDEF;
            if (first.tag() == V6Value.TAG_OBJ &&
                first.ref() instanceof V6Object &&
                !(first.ref() instanceof V6EventEmitterObject)) {
              V6Object opts = (V6Object)first.ref();
              V6Value inProp = opts.get("input");
              if (!inProp.isUndefined()) {
                inputVal = inProp;
                outputVal = opts.get("output");
              } else {
                inputVal = first;
              }
            } else {
              inputVal = first;
            }
            final V6Value fOutput = outputVal;
            final V6Value fInput = inputVal;

            V6EventEmitterObject rl = new V6EventEmitterObject();
            rl.setProto(V6EventEmitterConstructor.PROTOTYPE);

            StringBuilder pending = new StringBuilder();
            ArrayDeque<V6Callable> questionCallbacks = new ArrayDeque<>();

            V6Callable onData = (t, a) -> {
              V6Value chunkVal = V6Value.argAt(a, 0);
              String chunk =
                  chunkVal.tag() == V6Value.TAG_OBJ && chunkVal.ref() instanceof
                                                           V6Buffer
                      ? V6BufferConstructor.encodeBytes(
                            ((V6Buffer)chunkVal.ref()).toBytes(), "utf8")
                      : chunkVal.toString();
              pending.append(chunk);
              int idx;
              while ((idx = pending.indexOf("\n")) >= 0) {
                String line = pending.substring(0, idx);
                if (line.endsWith("\r"))
                  line = line.substring(0, line.length() - 1);
                pending.delete(0, idx + 1);
                V6Callable qcb = questionCallbacks.poll();
                if (qcb != null)
                  qcb.call(UNDEF, new V6Value[] {str(line)});
                else
                  rl.get("emit").asCallable().call(
                      objValue(rl), new V6Value[] {str("line"), str(line)});
              }
              return UNDEF;
            };
            V6Value setEncodingFn = fInput.getProp("setEncoding");
            if (setEncodingFn.tag() == V6Value.TAG_FUNC)
              setEncodingFn.asCallable().call(fInput,
                                              new V6Value[] {str("utf8")});

            fInput.getProp("on").asCallable().call(
                fInput, new V6Value[] {str("data"), fn(onData)});

            V6Value resumeFn = fInput.getProp("resume");
            if (resumeFn.tag() == V6Value.TAG_FUNC)
              resumeFn.asCallable().call(fInput, new V6Value[0]);

            rl.set("question", fn((t, a) -> {
                     String query = V6Value.argAt(a, 0).toString();
                     V6Callable cb =
                         a.length > 1 && a[1].tag() == V6Value.TAG_FUNC
                             ? a[1].asCallable()
                             : null;
                     if (fOutput.tag() == V6Value.TAG_OBJ) {
                       V6Value writeFn = fOutput.getProp("write");
                       if (writeFn.tag() == V6Value.TAG_FUNC)
                         writeFn.asCallable().call(fOutput,
                                                   new V6Value[] {str(query)});
                     }
                     if (cb != null)
                       questionCallbacks.add(cb);
                     return UNDEF;
                   }));

            rl.set("close", fn((t, a) -> {
                     rl.get("emit").asCallable().call(
                         objValue(rl), new V6Value[] {str("close")});
                     return UNDEF;
                   }));

            rl.set("write", fn((t, a) -> {
                     if (fOutput.tag() == V6Value.TAG_OBJ) {
                       V6Value writeFn = fOutput.getProp("write");
                       if (writeFn.tag() == V6Value.TAG_FUNC)
                         writeFn.asCallable().call(
                             fOutput, new V6Value[] {V6Value.argAt(a, 0)});
                     }
                     return UNDEF;
                   }));

            return objValue(rl);
          }));

    return o;
  }
}
