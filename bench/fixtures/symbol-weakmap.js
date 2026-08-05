function run() {
  const tagSymbol = Symbol("tag");
  const wm = new WeakMap();
  let total = 0;

  for (let i = 0; i < 50000; i++) {
    const obj = { [tagSymbol]: i, value: i * 2 };
    wm.set(obj, "meta-" + i);
    total += obj[tagSymbol] + obj.value;
    if (wm.has(obj)) {
      total += wm.get(obj).length;
    }
  }

  return total;
}

console.log(run());
