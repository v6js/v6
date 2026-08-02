async function readAll(readable) {
  const reader = readable.getReader();
  const chunks = [];
  while (true) {
    const { value, done } = await reader.read();
    if (done) break;
    chunks.push(value);
  }
  return Buffer.concat(chunks);
}

async function roundTrip(format) {
  const cs = new CompressionStream(format);
  const writer = cs.writable.getWriter();
  writer.write(Buffer.from("hello compression world, this text should compress ok"));
  writer.close();
  const compressed = await readAll(cs.readable);

  const ds = new DecompressionStream(format);
  const dwriter = ds.writable.getWriter();
  dwriter.write(compressed);
  dwriter.close();
  const decompressed = await readAll(ds.readable);

  console.log(format, "compressed size:", compressed.length, "roundtrip ok:",
              decompressed.toString() === "hello compression world, this text should compress ok");
}

roundTrip("gzip").then(() => roundTrip("deflate")).then(() => roundTrip("deflate-raw"));
