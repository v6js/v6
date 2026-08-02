console.log(typeof crypto.getRandomValues, typeof crypto.randomUUID, typeof crypto.subtle);

const arr = Buffer.alloc(16);
crypto.getRandomValues(arr);
console.log(arr.length, [...arr].some((b) => b !== 0));

console.log(/^[0-9a-f-]{36}$/.test(crypto.randomUUID()));

async function testDigest() {
  const enc = new TextEncoder();
  const data = enc.encode("hello world");
  const hashBuf = await crypto.subtle.digest("SHA-256", data);
  console.log("digest byteLength:", hashBuf.byteLength);
  console.log("digest hex:", Buffer.from(hashBuf).toString("hex"));
}

async function testAesGcm() {
  const key = await crypto.subtle.generateKey({ name: "AES-GCM", length: 256 }, true, ["encrypt", "decrypt"]);
  console.log("key type:", key.type, key.algorithm.name, key.extractable);
  const iv = crypto.getRandomValues(Buffer.alloc(12));
  const enc = new TextEncoder();
  const plaintext = enc.encode("secret message");
  const ciphertext = await crypto.subtle.encrypt({ name: "AES-GCM", iv }, key, plaintext);
  const decrypted = await crypto.subtle.decrypt({ name: "AES-GCM", iv }, key, ciphertext);
  const dec = new TextDecoder();
  console.log("aes-gcm roundtrip:", dec.decode(Buffer.from(decrypted)));

  const raw = await crypto.subtle.exportKey("raw", key);
  console.log("exported key length:", raw.byteLength);
  const imported = await crypto.subtle.importKey("raw", raw, { name: "AES-GCM" }, true, ["encrypt", "decrypt"]);
  const ciphertext2 = await crypto.subtle.encrypt({ name: "AES-GCM", iv }, imported, plaintext);
  console.log("imported key encrypt matches:", Buffer.from(ciphertext).equals(Buffer.from(ciphertext2)));
}

async function testHmac() {
  const key = await crypto.subtle.generateKey({ name: "HMAC", hash: "SHA-256" }, true, ["sign", "verify"]);
  const enc = new TextEncoder();
  const data = enc.encode("message to authenticate");
  const sig = await crypto.subtle.sign("HMAC", key, data);
  const valid = await crypto.subtle.verify("HMAC", key, sig, data);
  console.log("hmac valid:", valid);
  const tamperedData = enc.encode("tampered message");
  const invalid = await crypto.subtle.verify("HMAC", key, sig, tamperedData);
  console.log("hmac correctly rejects tampered:", !invalid);
}

async function testRsa() {
  const keyPair = await crypto.subtle.generateKey(
    { name: "RSASSA-PKCS1-v1_5", modulusLength: 2048, hash: "SHA-256" },
    true,
    ["sign", "verify"]
  );
  console.log("keypair types:", keyPair.publicKey.type, keyPair.privateKey.type);
  const enc = new TextEncoder();
  const data = enc.encode("rsa signed message");
  const sig = await crypto.subtle.sign({ name: "RSASSA-PKCS1-v1_5" }, keyPair.privateKey, data);
  const valid = await crypto.subtle.verify({ name: "RSASSA-PKCS1-v1_5" }, keyPair.publicKey, sig, data);
  console.log("rsa sign/verify valid:", valid);

  const spki = await crypto.subtle.exportKey("spki", keyPair.publicKey);
  console.log("spki export length > 0:", spki.byteLength > 0);
}

testDigest()
  .then(testAesGcm)
  .then(testHmac)
  .then(testRsa)
  .then(() => console.log("all done"))
  .catch((e) => console.log("ERROR:", e));
