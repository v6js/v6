function point(x, y, z) {
  return { x, y, z };
}

function run() {
  let total = 0;
  for (let i = 0; i < 100000; i++) {
    const { x, y, z } = point(i, i + 1, i + 2);
    const arr = [x, y, z];
    const [a, b, ...rest] = arr;
    const merged = { ...point(a, b, rest[0]), extra: 1 };
    total += merged.x + merged.y + merged.z + merged.extra;
  }
  return total;
}

console.log(run());
