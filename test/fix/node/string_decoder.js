const { StringDecoder } = require("string_decoder");

const dec = new StringDecoder("utf8");
const buf1 = Buffer.from([0xe2, 0x82]);
const buf2 = Buffer.from([0xac]);
const part1 = dec.write(buf1);
const part2 = dec.write(buf2);
console.log(part1.length, part2.length);
console.log((part1 + part2).charCodeAt(0) === 0x20ac);

const dec2 = new StringDecoder("utf8");
console.log(dec2.write(Buffer.from("hello")));
console.log(dec2.end());
