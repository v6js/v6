let i = 0;
while (i < 10) {
  if (i == 5) {
    i = i + 1;
    continue;
  }
  if (i == 8) {
    break;
  }
  console.log(i);
  i = i + 1;
}

for (let j = 0; j < 5; j++) {
  if (j == 2) continue;
  if (j == 4) break;
  console.log("j=" + j);
}

function sumTo(n) {
  let total = 0;
  let k = 1;
  while (k <= n) {
    total = total + k;
    k = k + 1;
  }
  return total;
}

console.log(sumTo(10));
console.log(sumTo(100));
