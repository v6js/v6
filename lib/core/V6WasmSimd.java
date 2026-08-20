import jdk.incubator.vector.ByteVector;
import jdk.incubator.vector.FloatVector;
import jdk.incubator.vector.IntVector;
import jdk.incubator.vector.VectorSpecies;

public final class V6WasmSimd {
  static final VectorSpecies<Byte> I8_SPECIES = ByteVector.SPECIES_128;
  static final VectorSpecies<Integer> I32_SPECIES = IntVector.SPECIES_128;
  static final VectorSpecies<Float> F32_SPECIES = FloatVector.SPECIES_128;

  private static IntVector loadI32(V6WasmV128 v) {
    return ByteVector.fromArray(I8_SPECIES, v.bytes, 0).reinterpretAsInts();
  }

  private static V6WasmV128 storeI32(IntVector v) {
    byte[] out = new byte[16];
    v.reinterpretAsBytes().intoArray(out, 0);
    return new V6WasmV128(out);
  }

  private static FloatVector loadF32(V6WasmV128 v) {
    return ByteVector.fromArray(I8_SPECIES, v.bytes, 0).reinterpretAsFloats();
  }

  private static V6WasmV128 storeF32(FloatVector v) {
    byte[] out = new byte[16];
    v.reinterpretAsBytes().intoArray(out, 0);
    return new V6WasmV128(out);
  }

  public static V6WasmV128 i32x4Splat(int x) {
    return storeI32(IntVector.broadcast(I32_SPECIES, x));
  }

  public static V6WasmV128 f32x4Splat(float x) {
    return storeF32(FloatVector.broadcast(F32_SPECIES, x));
  }

  public static V6WasmV128 i32x4Add(V6WasmV128 a, V6WasmV128 b) {
    return storeI32(loadI32(a).add(loadI32(b)));
  }

  public static V6WasmV128 i32x4Sub(V6WasmV128 a, V6WasmV128 b) {
    return storeI32(loadI32(a).sub(loadI32(b)));
  }

  public static V6WasmV128 i32x4Mul(V6WasmV128 a, V6WasmV128 b) {
    return storeI32(loadI32(a).mul(loadI32(b)));
  }

  public static V6WasmV128 f32x4Add(V6WasmV128 a, V6WasmV128 b) {
    return storeF32(loadF32(a).add(loadF32(b)));
  }

  public static V6WasmV128 f32x4Sub(V6WasmV128 a, V6WasmV128 b) {
    return storeF32(loadF32(a).sub(loadF32(b)));
  }

  public static V6WasmV128 f32x4Mul(V6WasmV128 a, V6WasmV128 b) {
    return storeF32(loadF32(a).mul(loadF32(b)));
  }

  public static V6WasmV128 f32x4Div(V6WasmV128 a, V6WasmV128 b) {
    return storeF32(loadF32(a).div(loadF32(b)));
  }

  public static int i32x4ExtractLane(V6WasmV128 v, int lane) {
    return loadI32(v).lane(lane);
  }

  public static float f32x4ExtractLane(V6WasmV128 v, int lane) {
    return loadF32(v).lane(lane);
  }

  private static ByteVector loadBytes(V6WasmV128 v) {
    return ByteVector.fromArray(I8_SPECIES, v.bytes, 0);
  }

  private static V6WasmV128 storeBytes(ByteVector v) {
    byte[] out = new byte[16];
    v.intoArray(out, 0);
    return new V6WasmV128(out);
  }

  public static V6WasmV128 v128Not(V6WasmV128 a) {
    return storeBytes(loadBytes(a).not());
  }

  public static V6WasmV128 v128And(V6WasmV128 a, V6WasmV128 b) {
    return storeBytes(loadBytes(a).and(loadBytes(b)));
  }

  public static V6WasmV128 v128Andnot(V6WasmV128 a, V6WasmV128 b) {
    return storeBytes(loadBytes(a).and(loadBytes(b).not()));
  }

  public static V6WasmV128 v128Or(V6WasmV128 a, V6WasmV128 b) {
    return storeBytes(loadBytes(a).or(loadBytes(b)));
  }

  public static V6WasmV128 v128Xor(V6WasmV128 a, V6WasmV128 b) {
    return storeBytes(loadBytes(a).lanewise(jdk.incubator.vector.VectorOperators.XOR, loadBytes(b)));
  }

  public static V6WasmV128 v128Bitselect(V6WasmV128 a, V6WasmV128 b, V6WasmV128 c) {
    ByteVector va = loadBytes(a);
    ByteVector vb = loadBytes(b);
    ByteVector vc = loadBytes(c);
    return storeBytes(va.and(vc).or(vb.and(vc.not())));
  }

  public static V6WasmV128 f32x4ConvertI32x4S(V6WasmV128 a) {
    FloatVector v = (FloatVector) loadI32(a).convert(jdk.incubator.vector.VectorOperators.I2F, 0);
    return storeF32(v);
  }

  public static V6WasmV128 i8x16Shuffle(V6WasmV128 a, V6WasmV128 b, byte[] indices) {
    byte[] out = new byte[16];
    for (int i = 0; i < 16; i++) {
      int idx = indices[i] & 0xFF;
      out[i] = idx < 16 ? a.bytes[idx] : b.bytes[idx - 16];
    }
    return new V6WasmV128(out);
  }

  public static V6WasmV128 load(V6WasmMemory mem, int addr) {
    byte[] out = new byte[16];
    mem.copyOut(addr, out, 0, 16);
    return new V6WasmV128(out);
  }

  public static void store(V6WasmMemory mem, int addr, V6WasmV128 v) {
    mem.copyIn(addr, v.bytes, 0, 16);
  }
}
