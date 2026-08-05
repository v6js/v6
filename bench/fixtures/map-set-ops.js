function run() {
  const map = new Map();
  const set = new Set();

  for (let i = 0; i < 50000; i++) {
    map.set(i, i * 2);
    set.add(i % 10000);
  }

  let total = 0;
  for (let i = 0; i < 50000; i++) {
    if (map.has(i)) {
      total += map.get(i);
    }
    if (set.has(i % 10000)) {
      total += 1;
    }
  }

  return total;
}

console.log(run());
