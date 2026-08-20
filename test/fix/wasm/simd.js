const fs = require("fs");
const path = require("path");

const bytes = fs.readFileSync(path.join(__dirname, "simd.wasm"));

WebAssembly.instantiate(bytes).then((result) => {
  const e = result.instance.exports;
  console.log(e.f32_ops(2, 3));
  console.log(e.f32_ops(10, -4));
  console.log(e.i32_ops(6, 7));
  console.log(e.i32_ops(-5, 12));
  console.log(e.bitwise_ops(0xf0f0, 0x0ff0));
  console.log(e.bitwise_ops(-1, 0x12345678));
  console.log(e.dot4(1, 2, 3, 4, 5, 6, 7, 8));
  console.log(e.dot4(-1, 0.5, 2, -3, 4, -2, 1, 0));
});
