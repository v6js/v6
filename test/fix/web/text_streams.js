async function testEncoderStream() {
  const es = new TextEncoderStream();
  const writer = es.writable.getWriter();
  const reader = es.readable.getReader();
  writer.write("hi").then(() => writer.close());
  const chunks = [];
  while (true) {
    const { value, done } = await reader.read();
    if (done) break;
    chunks.push(value);
  }
  console.log("encoder chunks:", chunks.length, Buffer.isBuffer(chunks[0]));
  console.log("encoder bytes:", [...chunks[0]].join(","));
}

async function testDecoderStream() {
  const ds = new TextDecoderStream();
  const writer = ds.writable.getWriter();
  const reader = ds.readable.getReader();
  const enc = new TextEncoder();
  writer.write(enc.encode("hello")).then(() => writer.close());
  let out = "";
  while (true) {
    const { value, done } = await reader.read();
    if (done) break;
    out += value;
  }
  console.log("decoder result:", out);
}

testEncoderStream().then(testDecoderStream);
