const blob = new Blob(["hello ", "world"], { type: "text/plain" });
console.log(blob.size, blob.type);

blob.text().then((t) => console.log("text:", t));
blob.arrayBuffer().then((ab) => console.log("arrayBuffer byteLength:", ab.byteLength));
blob.bytes().then((b) => console.log("bytes isBuffer:", Buffer.isBuffer(b), b.length));

const sliced = blob.slice(0, 5);
sliced.text().then((t) => console.log("sliced text:", t));

const streamed = blob.stream();
const reader = streamed.getReader();
reader.read().then(({ value, done }) => {
  console.log("stream chunk isBuffer:", Buffer.isBuffer(value), done);
});

const file = new File(["file content"], "test.txt", { type: "text/plain", lastModified: 12345 });
console.log(file.name, file.type, file.size, file.lastModified);
file.text().then((t) => console.log("file text:", t));
console.log(file instanceof Blob);

const ab = new ArrayBuffer(4);
console.log(ab.byteLength);
const ab2 = ab.slice(1, 3);
console.log(ab2.byteLength);

const blobFromBuffer = new Blob([Buffer.from("abc")]);
blobFromBuffer.text().then((t) => console.log("from buffer:", t));
