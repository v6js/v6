function run() {
  let total = 0;

  for (let i = 0; i < 4000; i++) {
    const arr = Array.from({ length: 50 }, (_, idx) => idx * i);
    const set = new Set(arr);
    const fromSet = Array.from(set);
    total += fromSet.length;
  }

  return total;
}

console.log(run());
