function mayThrow(n) {
  if (n % 7 === 0) {
    throw new Error("divisible by seven: " + n);
  }
  return n * 2;
}

function run() {
  let total = 0;
  let caught = 0;
  for (let i = 0; i < 100000; i++) {
    try {
      total += mayThrow(i);
    } catch (e) {
      caught += 1;
    }
  }
  return total + caught;
}

console.log(run());
