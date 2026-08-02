const zlib = require("zlib");

const input = "hello world hello world hello world";

const gz = zlib.gzipSync(input);
console.log(Buffer.isBuffer(gz));
console.log(zlib.gunzipSync(gz).toString());

const def = zlib.deflateSync(input);
console.log(zlib.inflateSync(def).toString());

const defRaw = zlib.deflateRawSync(input);
console.log(zlib.inflateRawSync(defRaw).toString());

zlib.gzip(input, (err, buf) => {
  console.log("async gzip err:", err);
  zlib.gunzip(buf, (err2, out) => console.log("async gunzip:", out.toString()));
});

try {
  zlib.gunzipSync(Buffer.from("not gzip data"));
  console.log("should not reach");
} catch (e) {
  console.log("caught bad gzip data");
}
