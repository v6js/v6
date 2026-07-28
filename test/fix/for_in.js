var o = { a: 1, b: 2, c: 3 };
for (var k in o) {
  print(k);
  print(o[k]);
}

var arr = [10, 20, 30];
for (var i in arr) {
  print(i);
  print(arr[i]);
}

for (let key in o) {
  if (key == "b") continue;
  print(key);
}

var count = 0;
for (var m in o) {
  if (m == "b") break;
  count = count + 1;
}
print(count);
