var arr = [3, 1, 2];
arr.push(4);
print(arr.length);
print(arr[3]);
print(arr.pop());
print(arr.length);

var a2 = [1, 2, 3];
print(a2.shift());
print(a2[0]);
a2.unshift(0);
print(a2[0]);
print(a2.length);

var a3 = [1, 2, 3, 4, 5];
var sliced = a3.slice(1, 3);
print(sliced.length);
print(sliced[0]);
print(sliced[1]);

print(a3.indexOf(3));
print(a3.includes(3));
print(a3.includes(99));
print(a3.join("-"));

var doubled = a3.map(function(x) { return x * 2; });
print(doubled[0]);
print(doubled[4]);

var evens = a3.filter(function(x) { return x % 2 === 0; });
print(evens.length);
print(evens[0]);

var total = 0;
a3.forEach(function(x) { total = total + x; });
print(total);

var sum = a3.reduce(function(acc, x) { return acc + x; }, 0);
print(sum);

var noInit = [1, 2, 3].reduce(function(acc, x) { return acc + x; });
print(noInit);

var combined = [1, 2].concat([3, 4], 5);
print(combined.length);
print(combined[4]);

var rev = [1, 2, 3].reverse();
print(rev[0]);
print(rev[2]);

var sorted = [3, 1, 2].sort();
print(sorted[0]);
print(sorted[2]);

var sortedDesc = [3, 1, 2].sort(function(a, b) { return b - a; });
print(sortedDesc[0]);
print(sortedDesc[2]);

var obj = { b: 2, a: 1 };
var keys = Object.keys(obj);
print(keys.length);
print(keys[0]);

var values = Object.values(obj);
print(values[0]);

var entries = Object.entries(obj);
print(entries.length);
print(entries[0][0]);
print(entries[0][1]);

var target = { x: 1 };
Object.assign(target, { y: 2 }, { z: 3 });
print(target.x);
print(target.y);
print(target.z);

var frozen = { a: 1 };
Object.freeze(frozen);
frozen.a = 99;
print(frozen.a);
print(Object.isFrozen(frozen));

var proto = { greet: function() { return "hi"; } };
var created = Object.create(proto);
print(created.greet());

var fromE = Object.fromEntries([["a", 1], ["b", 2]]);
print(fromE.a);
print(fromE.b);

print(Object.is(1, 1));
print(Object.is(1, 2));

print(Array.isArray([1, 2]));
print(Array.isArray("no"));

var fromArr = Array.from("abc");
print(fromArr.length);
print(fromArr[0]);

var ofArr = Array.of(1, 2, 3);
print(ofArr.length);

var encoded = btoa("hello");
print(encoded);
print(atob(encoded));
