const crypto = require("crypto");

const h = crypto.createHash("sha256");
h.update("hello");
console.log(h.digest("hex"));

console.log(crypto.createHash("md5").update("hello").digest("hex"));

const hmac = crypto.createHmac("sha256", "secret-key");
hmac.update("message");
console.log(hmac.digest("hex"));

const b = crypto.randomBytes(16);
console.log(Buffer.isBuffer(b));
console.log(b.length);

const uuid = crypto.randomUUID();
console.log(/^[0-9a-f-]{36}$/.test(uuid));

const n = crypto.randomInt(10, 20);
console.log(n >= 10 && n < 20);

crypto.randomBytes(8, (err, buf) => {
  console.log("async randomBytes err:", err);
  console.log("async randomBytes len:", buf.length);
});

const cbcKey = Buffer.alloc(32, 7);
const cbcIv = Buffer.alloc(16, 9);
const cipher = crypto.createCipheriv("aes-256-cbc", cbcKey, cbcIv);
let enc = cipher.update("hello world", "utf8", "hex");
enc += cipher.final("hex");
console.log("cbc enc:", enc);
const decipher = crypto.createDecipheriv("aes-256-cbc", cbcKey, cbcIv);
let dec = decipher.update(enc, "hex", "utf8");
dec += decipher.final("utf8");
console.log("cbc dec:", dec);

const gcmKey = Buffer.alloc(32, 3);
const gcmIv = Buffer.alloc(12, 5);
const gcmCipher = crypto.createCipheriv("aes-256-gcm", gcmKey, gcmIv);
let gcmEnc = gcmCipher.update("secret data", "utf8", "hex");
gcmEnc += gcmCipher.final("hex");
const authTag = gcmCipher.getAuthTag();
console.log("gcm enc:", gcmEnc, authTag.toString("hex"));
const gcmDecipher = crypto.createDecipheriv("aes-256-gcm", gcmKey, gcmIv);
gcmDecipher.setAuthTag(authTag);
let gcmDec = gcmDecipher.update(gcmEnc, "hex", "utf8");
gcmDec += gcmDecipher.final("utf8");
console.log("gcm dec:", gcmDec);

const { publicKey, privateKey } = crypto.generateKeyPairSync("rsa", { modulusLength: 2048 });
console.log("has pub:", publicKey.startsWith("-----BEGIN PUBLIC KEY-----"));
console.log("has priv:", privateKey.startsWith("-----BEGIN PRIVATE KEY-----"));

const sign = crypto.createSign("RSA-SHA256");
sign.update("message to sign");
const signature = sign.sign(privateKey, "hex");
const verify = crypto.createVerify("RSA-SHA256");
verify.update("message to sign");
console.log("verify ok:", verify.verify(publicKey, signature, "hex"));
const badVerify = crypto.createVerify("RSA-SHA256");
badVerify.update("tampered message");
console.log("verify bad:", badVerify.verify(publicKey, signature, "hex"));

console.log("pbkdf2:", crypto.pbkdf2Sync("password", "salt", 1000, 32, "sha256").toString("hex"));
crypto.pbkdf2("password", "salt", 1000, 32, "sha256", (err, derived) => {
  console.log("pbkdf2 async:", derived.toString("hex"));
});

console.log("timingSafeEqual same:", crypto.timingSafeEqual(Buffer.from("abc"), Buffer.from("abc")));
console.log("timingSafeEqual diff:", crypto.timingSafeEqual(Buffer.from("abc"), Buffer.from("abd")));
