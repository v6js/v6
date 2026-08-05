function run() {
  let x = 0x12345678;
  let total = 0;
  for (let i = 0; i < 500000; i++) {
    x = ((x << 1) ^ (x >>> 3)) & 0xffffffff;
    x = x | (i & 0xff);
    x = x ^ (i << 2);
    total += x & 0xf;
  }
  return total;
}

console.log(run());
