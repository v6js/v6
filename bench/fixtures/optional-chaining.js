function makeChain(i) {
  if (i % 4 === 0) return {};
  if (i % 4 === 1) return { a: {} };
  if (i % 4 === 2) return { a: { b: {} } };
  return { a: { b: { c: i } } };
}

function run() {
  let total = 0;
  for (let i = 0; i < 100000; i++) {
    const obj = makeChain(i);
    total += obj?.a?.b?.c ?? -1;
  }
  return total;
}

console.log(run());
