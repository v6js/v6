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
