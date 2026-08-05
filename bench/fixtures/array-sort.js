function run() {
  const arr = [];
  let seed = 42;
  for (let i = 0; i < 100000; i++) {
    seed = (seed * 1103515245 + 12345) & 0x7fffffff;
    arr.push(seed % 1000000);
  }

  arr.sort((a, b) => a - b);

  let sum = 0;
  for (let i = 0; i < arr.length; i++) {
    sum += arr[i];
  }
  return sum;
}

console.log(run());
