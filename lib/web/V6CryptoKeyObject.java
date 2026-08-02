public final class V6CryptoKeyObject extends V6Object {
  Object keyMaterial;
  String algorithmName = "";
  V6Object algorithmObj;
  String type = "secret";
  boolean extractable = false;
  V6Array usages = new V6Array();
}
