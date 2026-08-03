let o = { a: 1, b: 2, c: 3 };
for (let k in o) {
  console.log(k);
  console.log(o[k]);
}

let arr = [10, 20, 30];
for (let i in arr) {
  console.log(i);
  console.log(arr[i]);
}

for (let key in o) {
  if (key == "b") continue;
  console.log(key);
}

let count = 0;
for (let m in o) {
  if (m == "b") break;
  count = count + 1;
}
console.log(count);
