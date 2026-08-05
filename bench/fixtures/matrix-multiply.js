function makeMatrix(n, fill) {
  const m = [];
  for (let i = 0; i < n; i++) {
    const row = [];
    for (let j = 0; j < n; j++) {
      row.push(fill(i, j));
    }
    m.push(row);
  }
  return m;
}

function multiply(a, b, n) {
  const result = makeMatrix(n, () => 0);
  for (let i = 0; i < n; i++) {
    for (let k = 0; k < n; k++) {
      const aik = a[i][k];
      for (let j = 0; j < n; j++) {
        result[i][j] += aik * b[k][j];
      }
    }
  }
  return result;
}

function run() {
  const n = 45;
  const a = makeMatrix(n, (i, j) => i + j);
  const b = makeMatrix(n, (i, j) => i - j);
  const result = multiply(a, b, n);

  let total = 0;
  for (let i = 0; i < n; i++) {
    for (let j = 0; j < n; j++) {
      total += result[i][j];
    }
  }
  return total;
}

console.log(run());
