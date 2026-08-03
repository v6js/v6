let arr = [10, 20, 30];
for (let x of arr) {
  console.log(x);
}

for (let c of "hi") {
  console.log(c);
}

let sum = 0;
for (let y of arr) {
  if (y == 20) continue;
  sum = sum + y;
}
console.log(sum);

for (let z of arr) {
  if (z == 20) break;
  console.log(z);
}
