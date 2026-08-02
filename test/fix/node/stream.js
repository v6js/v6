const { Readable, Writable, Transform } = require("stream");

const r = new Readable();
let received = [];
r.on("data", (chunk) => received.push(chunk));
r.on("end", () => console.log("readable end, chunks:", received));
r.push("a");
r.push("b");
r.push(null);

class Collector extends Writable {
  constructor() {
    super();
    this.chunks = [];
  }
  _write(chunk, encoding, callback) {
    this.chunks.push(chunk);
    callback();
  }
}
const w = new Collector();
w.on("finish", () => console.log("writable finished, chunks:", w.chunks));
w.write("x");
w.write("y");
w.end("z");

const r2 = new Readable();
const w2 = new Collector();
r2.pipe(w2);
r2.push("piped-1");
r2.push("piped-2");
r2.push(null);
console.log("piped chunks:", w2.chunks);

class Upper extends Transform {
  _transform(chunk, encoding, callback) {
    callback(null, chunk.toString().toUpperCase());
  }
}
const t = new Upper();
let upperChunks = [];
t.on("data", (c) => upperChunks.push(c));
t.on("end", () => console.log("transform end:", upperChunks));
t.write("hello");
t.write("world");
t.end();
