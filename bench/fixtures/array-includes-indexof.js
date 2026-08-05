function run() {
  const arr = [];
  for (let i = 0; i < 1200; i++) {
    arr.push(i);
  }

  let total = 0;
  for (let i = 0; i < 1200; i++) {
    const needle = (i * 37) % 1200;
    if (arr.includes(needle)) {
      total += 1;
    }
    total += arr.indexOf(needle) >= 0 ? 1 : 0;
  }
  return total;
}

console.log(run());
