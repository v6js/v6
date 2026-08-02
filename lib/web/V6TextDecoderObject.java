public final class V6TextDecoderObject extends V6Object {
  V6StringDecoderObject decoder;
  String encoding;
  boolean fatal;
  boolean ignoreBOM;
  boolean sawFirstChunk = false;
}
