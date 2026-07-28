var arr = [10, 20, 30];
for (var x of arr) {
  print(x);
}

for (let c of "hi") {
  print(c);
}

var sum = 0;
for (var y of arr) {
  if (y == 20) continue;
  sum = sum + y;
}
print(sum);

for (var z of arr) {
  if (z == 20) break;
  print(z);
}
