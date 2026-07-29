let arr = [3, 1, 2];
arr.push(4);
console.log(arr.length);
console.log(arr[3]);
console.log(arr.pop());
console.log(arr.length);

let a2 = [1, 2, 3];
console.log(a2.shift());
console.log(a2[0]);
a2.unshift(0);
console.log(a2[0]);
console.log(a2.length);

let a3 = [1, 2, 3, 4, 5];
let sliced = a3.slice(1, 3);
console.log(sliced.length);
console.log(sliced[0]);
console.log(sliced[1]);

console.log(a3.indexOf(3));
console.log(a3.includes(3));
console.log(a3.includes(99));
console.log(a3.join("-"));

let doubled = a3.map(function(x) { return x * 2; });
console.log(doubled[0]);
console.log(doubled[4]);

let evens = a3.filter(function(x) { return x % 2 === 0; });
console.log(evens.length);
console.log(evens[0]);

let total = 0;
a3.forEach(function(x) { total = total + x; });
console.log(total);

let sum = a3.reduce(function(acc, x) { return acc + x; }, 0);
console.log(sum);

let noInit = [1, 2, 3].reduce(function(acc, x) { return acc + x; });
console.log(noInit);

let combined = [1, 2].concat([3, 4], 5);
console.log(combined.length);
console.log(combined[4]);

let rev = [1, 2, 3].reverse();
console.log(rev[0]);
console.log(rev[2]);

let sorted = [3, 1, 2].sort();
console.log(sorted[0]);
console.log(sorted[2]);

let sortedDesc = [3, 1, 2].sort(function(a, b) { return b - a; });
console.log(sortedDesc[0]);
console.log(sortedDesc[2]);

let obj = { b: 2, a: 1 };
let keys = Object.keys(obj);
console.log(keys.length);
console.log(keys[0]);

let values = Object.values(obj);
console.log(values[0]);

let entries = Object.entries(obj);
console.log(entries.length);
console.log(entries[0][0]);
console.log(entries[0][1]);

let target = { x: 1 };
Object.assign(target, { y: 2 }, { z: 3 });
console.log(target.x);
console.log(target.y);
console.log(target.z);

let frozen = { a: 1 };
Object.freeze(frozen);
frozen.a = 99;
console.log(frozen.a);
console.log(Object.isFrozen(frozen));

let proto = { greet: function() { return "hi"; } };
let created = Object.create(proto);
console.log(created.greet());

let fromE = Object.fromEntries([["a", 1], ["b", 2]]);
console.log(fromE.a);
console.log(fromE.b);

console.log(Object.is(1, 1));
console.log(Object.is(1, 2));

console.log(Array.isArray([1, 2]));
console.log(Array.isArray("no"));

let fromArr = Array.from("abc");
console.log(fromArr.length);
console.log(fromArr[0]);

let ofArr = Array.of(1, 2, 3);
console.log(ofArr.length);

let encoded = btoa("hello");
console.log(encoded);
console.log(atob(encoded));
