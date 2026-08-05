function* range(start, end) {
  for (let i = start; i < end; i++) {
    yield i * i;
  }
}

function run() {
  let total = 0;
  for (let i = 0; i < 50; i++) {
    for (const v of range(0, 2000)) {
      total += v;
    }
  }
  return total;
}

console.log(run());
