import jdk.incubator.vector.ByteVector;
import jdk.incubator.vector.VectorSpecies;

public final class V6TypedArraySimd {
  private static final VectorSpecies<Byte> SPECIES =
      ByteVector.SPECIES_PREFERRED;

  public static void fill(byte[] data, int start, int end, byte value) {
    int i = start;
    int upper = SPECIES.loopBound(end - start) + start;
    ByteVector bv = ByteVector.broadcast(SPECIES, value);
    for (; i < upper; i += SPECIES.length())
      bv.intoArray(data, i);
    for (; i < end; i++)
      data[i] = value;
  }
}
