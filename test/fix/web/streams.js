const rs = new ReadableStream({
  start(controller) {
    controller.enqueue("a");
    controller.enqueue("b");
    controller.enqueue("c");
    controller.close();
  },
});
const reader = rs.getReader();
const chunks = [];
function pump() {
  return reader.read().then(({ value, done }) => {
    if (done) return;
    chunks.push(value);
    return pump();
  });
}
pump().then(() => console.log("readable basic:", chunks.join(",")));

const pullStream = new ReadableStream({
  start(controller) { controller._n = 0; },
  pull(controller) {
    controller._n = (controller._n || 0) + 1;
    if (controller._n > 3) { controller.close(); return; }
    controller.enqueue(controller._n);
  },
});
const pullReader = pullStream.getReader();
const pulled = [];
function pump2() {
  return pullReader.read().then(({ value, done }) => {
    if (done) return;
    pulled.push(value);
    return pump2();
  });
}
pump2().then(() => console.log("readable pull:", pulled.join(",")));

const written = [];
const ws = new WritableStream({
  write(chunk) { written.push(chunk); },
  close() { written.push("CLOSED"); },
});
const writer = ws.getWriter();
writer.write("x").then(() => writer.write("y")).then(() => writer.write("z")).then(() => writer.close())
  .then(() => console.log("writable basic:", written.join(",")));

const ts = new TransformStream({
  transform(chunk, controller) {
    controller.enqueue(chunk.toUpperCase());
  },
});
const tWriter = ts.writable.getWriter();
const tReader = ts.readable.getReader();
const transformed = [];
function pumpT() {
  return tReader.read().then(({ value, done }) => {
    if (done) return;
    transformed.push(value);
    return pumpT();
  });
}
Promise.all([
  tWriter.write("foo").then(() => tWriter.write("bar")).then(() => tWriter.close()),
  pumpT(),
]).then(() => console.log("transform:", transformed.join(",")));

const src = new ReadableStream({
  start(controller) {
    controller.enqueue(1);
    controller.enqueue(2);
    controller.close();
  },
});
const sink = [];
const destWs = new WritableStream({ write(c) { sink.push(c); } });
src.pipeTo(destWs).then(() => console.log("pipeTo:", sink.join(",")));

const teeSrc = new ReadableStream({
  start(controller) {
    controller.enqueue("t1");
    controller.enqueue("t2");
    controller.close();
  },
});
const [teeA, teeB] = teeSrc.tee();
async function drain(reader) {
  const out = [];
  while (true) {
    const { value, done } = await reader.read();
    if (done) break;
    out.push(value);
  }
  return out.join(",");
}
Promise.all([drain(teeA.getReader()), drain(teeB.getReader())]).then((results) => {
  console.log("tee:", results[0], results[1]);
});

const cqs = new CountQueuingStrategy({ highWaterMark: 5 });
console.log("count strategy:", cqs.highWaterMark, cqs.size());
const blqs = new ByteLengthQueuingStrategy({ highWaterMark: 10 });
console.log("byte strategy:", blqs.highWaterMark, blqs.size(Buffer.from("hello")));
