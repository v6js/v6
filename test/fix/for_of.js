var arr = [10, 20, 30];
for (var x of arr) {
  console.log(x);
}

for (let c of "hi") {
  console.log(c);
}

var sum = 0;
for (var y of arr) {
  if (y == 20) continue;
  sum = sum + y;
}
console.log(sum);

for (var z of arr) {
  if (z == 20) break;
  console.log(z);
}
