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

outer: for (let row = 0; row < 3; row++) {
  for (let col = 0; col < 3; col++) {
    if (col === 1) continue outer;
    console.log("cell", row, col);
  }
}

search: for (let row = 0; row < 5; row++) {
  for (let col = 0; col < 5; col++) {
    if (row === 2 && col === 2) break search;
    console.log("scan", row, col);
  }
}

let ticks = 0;
countdown: while (ticks < 5) {
  ticks++;
  if (ticks === 2) continue countdown;
  if (ticks === 4) break countdown;
  console.log("tick", ticks);
}
console.log("final ticks", ticks);

report: {
  console.log("before break");
  if (true) break report;
  console.log("unreachable");
}
console.log("after block");
