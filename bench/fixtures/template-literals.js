function run() {
  let total = 0;
  for (let i = 0; i < 100000; i++) {
    const name = "item" + i;
    const s = `${name}-${i}: value=${i * 2}, half=${i / 2}`;
    total += s.length;
  }
  return total;
}

console.log(run());
