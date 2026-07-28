function sumSquaresOfEvens(arr) {
  return arr
    .filter((x) => x % 2 === 0)
    .map((x) => x * x)
    .reduce((acc, x) => acc + x, 0);
}

function run() {
  const data = [];
  for (let i = 0; i < 2000000; i++) {
    data.push(i);
  }
  return sumSquaresOfEvens(data);
}

console.log(run());
