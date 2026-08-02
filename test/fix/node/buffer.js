const b1 = Buffer.from("hello");
console.log(b1.toString());
console.log(b1.length);
console.log(b1.toString("hex"));
console.log(b1.toString("base64"));

const b2 = Buffer.from(b1.toString("base64"), "base64");
console.log(b2.toString());

const b3 = Buffer.alloc(5, "ab");
console.log(b3.toString());

const b4 = Buffer.alloc(3);
console.log(b4.toString("hex"));

console.log(Buffer.isBuffer(b1));
console.log(Buffer.isBuffer("no"));
console.log(Buffer.byteLength("hello"));

const b5 = Buffer.concat([b1, Buffer.from(" world")]);
console.log(b5.toString());

console.log(b5.slice(0, 5).toString());
console.log(b5.slice(-5).toString());

console.log(b1.equals(Buffer.from("hello")));
console.log(b1.equals(Buffer.from("nope")));

console.log(JSON.stringify(Buffer.from("ab").toJSON()));

const b6 = Buffer.alloc(5);
const written = b6.write("hi");
console.log(written, b6.toString());

const b7 = new Buffer("direct");
console.log(b7.toString());
