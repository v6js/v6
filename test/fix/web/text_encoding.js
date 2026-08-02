const enc = new TextEncoder();
console.log(enc.encoding);
const bytes = enc.encode("hello");
console.log(Buffer.isBuffer(bytes), bytes.length, [...bytes].join(","));

const bytesUtf8 = enc.encode("café");
console.log(bytesUtf8.length, [...bytesUtf8].join(","));

const dest = Buffer.alloc(3);
const info = enc.encodeInto("hello", dest);
console.log(info.written, [...dest].join(","));

const dec = new TextDecoder();
console.log(dec.encoding, dec.fatal, dec.ignoreBOM);
console.log(dec.decode(bytes));
const decodedUtf8 = dec.decode(bytesUtf8);
console.log(decodedUtf8.length, decodedUtf8.charCodeAt(3) === 233);

const streamDec = new TextDecoder();
const full = enc.encode("streaming test");
let out = "";
for (let i = 0; i < full.length; i++) {
  out += streamDec.decode(Buffer.from([full[i]]), { stream: true });
}
out += streamDec.decode();
console.log(out);

const latin1Dec = new TextDecoder("latin1");
console.log(latin1Dec.encoding);
