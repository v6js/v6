function run() {
  const size = 500000;
  const buf = new Uint8Array(size);

  for (let i = 0; i < size; i++) {
    buf[i] = i & 0xff;
  }

  let total = 0;
  for (let i = 0; i < size; i++) {
    total += buf[i];
  }
  return total;
}

console.log(run());
