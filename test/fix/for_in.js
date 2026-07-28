var o = { a: 1, b: 2, c: 3 };
for (var k in o) {
  console.log(k);
  console.log(o[k]);
}

var arr = [10, 20, 30];
for (var i in arr) {
  console.log(i);
  console.log(arr[i]);
}

for (let key in o) {
  if (key == "b") continue;
  console.log(key);
}

var count = 0;
for (var m in o) {
  if (m == "b") break;
  count = count + 1;
}
console.log(count);
