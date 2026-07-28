var arr = [1, 2, 3, 4];
var [a, b] = arr;
console.log(a);
console.log(b);

var [x, , z] = arr;
console.log(x);
console.log(z);

var [first, ...rest] = arr;
console.log(first);
console.log(rest.length);
console.log(rest[0]);
console.log(rest[1]);

var [p = 10, q = 20] = [1];
console.log(p);
console.log(q);

var obj = { name: "Ada", age: 36 };
var { name, age } = obj;
console.log(name);
console.log(age);

var { name: n2, city = "unknown" } = obj;
console.log(n2);
console.log(city);

function sum3(a, b, c) {
  return a + b + c;
}
console.log(sum3(...[1, 2, 3]));

function total(...nums) {
  var s = 0;
  for (var i = 0; i < nums.length; i = i + 1) {
    s = s + nums[i];
  }
  return s;
}
console.log(total(1, 2, 3, 4, 5));
console.log(total());

var more = [10, 20];
console.log(total(1, ...more, 2));

var merged = [...arr, 5, 6];
console.log(merged.length);
console.log(merged[4]);
console.log(merged[5]);

var o1 = { a: 1, b: 2 };
var o2 = { ...o1, b: 3, c: 4 };
console.log(o2.a);
console.log(o2.b);
console.log(o2.c);
