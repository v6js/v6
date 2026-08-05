function run() {
  const obj = {};
  for (let i = 0; i < 200; i++) {
    obj["key" + i] = i;
  }

  let total = 0;
  for (let iter = 0; iter < 500; iter++) {
    for (const key in obj) {
      total += obj[key];
    }
  }

  return total;
}

console.log(run());
