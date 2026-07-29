let m = new Map();
m.set("a", 1);
m.set("b", 2);
console.log(m.get("a"));
console.log(m.get("c"));
console.log(m.has("b"));
console.log(m.size);
m.delete("a");
console.log(m.has("a"));
console.log(m.size);

let m2 = new Map([
  ["x", 10],
  ["y", 20],
]);
console.log(m2.get("x"), m2.get("y"));

let sum = 0;
m2.forEach((v) => {
  sum += v;
});
console.log(sum);

for (const k of m2.keys()) console.log(k);
for (const v of m2.values()) console.log(v);

let objectKey = {};
let m3 = new Map();
m3.set(objectKey, "stored");
console.log(m3.get(objectKey));
console.log(m3.get({}));

let numberSet = new Set([1, 2, 3, 2, 1]);
console.log(numberSet.size);
console.log(numberSet.has(2));
numberSet.add(4);
console.log(numberSet.size);
numberSet.delete(1);
console.log(numberSet.has(1));
console.log(numberSet.size);

let setSum = 0;
numberSet.forEach((v) => {
  setSum += v;
});
console.log(setSum);

for (const v of numberSet.values()) console.log(v);

let weakSet = new WeakSet();
let weakMap = new WeakMap();
let key1 = {};
weakMap.set(key1, "value1");
weakSet.add(key1);
console.log(weakMap.get(key1));
console.log(weakSet.has(key1));
