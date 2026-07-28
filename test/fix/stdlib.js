var arr = [3, 1, 2];
arr.push(4);
console.log(arr.length);
console.log(arr[3]);
console.log(arr.pop());
console.log(arr.length);

var a2 = [1, 2, 3];
console.log(a2.shift());
console.log(a2[0]);
a2.unshift(0);
console.log(a2[0]);
console.log(a2.length);

var a3 = [1, 2, 3, 4, 5];
var sliced = a3.slice(1, 3);
console.log(sliced.length);
console.log(sliced[0]);
console.log(sliced[1]);

console.log(a3.indexOf(3));
console.log(a3.includes(3));
console.log(a3.includes(99));
console.log(a3.join("-"));

var doubled = a3.map(function(x) { return x * 2; });
console.log(doubled[0]);
console.log(doubled[4]);

var evens = a3.filter(function(x) { return x % 2 === 0; });
console.log(evens.length);
console.log(evens[0]);

var total = 0;
a3.forEach(function(x) { total = total + x; });
console.log(total);

var sum = a3.reduce(function(acc, x) { return acc + x; }, 0);
console.log(sum);

var noInit = [1, 2, 3].reduce(function(acc, x) { return acc + x; });
console.log(noInit);

var combined = [1, 2].concat([3, 4], 5);
console.log(combined.length);
console.log(combined[4]);

var rev = [1, 2, 3].reverse();
console.log(rev[0]);
console.log(rev[2]);

var sorted = [3, 1, 2].sort();
console.log(sorted[0]);
console.log(sorted[2]);

var sortedDesc = [3, 1, 2].sort(function(a, b) { return b - a; });
console.log(sortedDesc[0]);
console.log(sortedDesc[2]);

var obj = { b: 2, a: 1 };
var keys = Object.keys(obj);
console.log(keys.length);
console.log(keys[0]);

var values = Object.values(obj);
console.log(values[0]);

var entries = Object.entries(obj);
console.log(entries.length);
console.log(entries[0][0]);
console.log(entries[0][1]);

var target = { x: 1 };
Object.assign(target, { y: 2 }, { z: 3 });
console.log(target.x);
console.log(target.y);
console.log(target.z);

var frozen = { a: 1 };
Object.freeze(frozen);
frozen.a = 99;
console.log(frozen.a);
console.log(Object.isFrozen(frozen));

var proto = { greet: function() { return "hi"; } };
var created = Object.create(proto);
console.log(created.greet());

var fromE = Object.fromEntries([["a", 1], ["b", 2]]);
console.log(fromE.a);
console.log(fromE.b);

console.log(Object.is(1, 1));
console.log(Object.is(1, 2));

console.log(Array.isArray([1, 2]));
console.log(Array.isArray("no"));

var fromArr = Array.from("abc");
console.log(fromArr.length);
console.log(fromArr[0]);

var ofArr = Array.of(1, 2, 3);
console.log(ofArr.length);

var encoded = btoa("hello");
console.log(encoded);
console.log(atob(encoded));
