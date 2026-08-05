function run() {
  let total = 0;
  const base = { a: 1, b: 2, c: 3 };

  for (let i = 0; i < 50000; i++) {
    const merged = Object.assign({}, base, { d: i, e: i * 2 });
    const keys = Object.keys(merged);
    total += keys.length + merged.d;
  }

  return total;
}

console.log(run());
