var arr = [1, 2, 3, 4];
var [a, b] = arr;
print(a);
print(b);

var [x, , z] = arr;
print(x);
print(z);

var [first, ...rest] = arr;
print(first);
print(rest.length);
print(rest[0]);
print(rest[1]);

var [p = 10, q = 20] = [1];
print(p);
print(q);

var obj = { name: "Ada", age: 36 };
var { name, age } = obj;
print(name);
print(age);

var { name: n2, city = "unknown" } = obj;
print(n2);
print(city);

function sum3(a, b, c) {
  return a + b + c;
}
print(sum3(...[1, 2, 3]));

function total(...nums) {
  var s = 0;
  for (var i = 0; i < nums.length; i = i + 1) {
    s = s + nums[i];
  }
  return s;
}
print(total(1, 2, 3, 4, 5));
print(total());

var more = [10, 20];
print(total(1, ...more, 2));

var merged = [...arr, 5, 6];
print(merged.length);
print(merged[4]);
print(merged[5]);

var o1 = { a: 1, b: 2 };
var o2 = { ...o1, b: 3, c: 4 };
print(o2.a);
print(o2.b);
print(o2.c);
