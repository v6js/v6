let x = 10;
x += 5;
console.log(x);
x -= 3;
console.log(x);
x *= 2;
console.log(x);
x /= 4;
console.log(x);
x %= 4;
console.log(x);

let k = 0;
console.log(k++);
console.log(k);
console.log(++k);
console.log(k);

console.log(1 < 2 ? "less" : "not less");
console.log(5 > 10 ? "gt" : "not gt");

console.log("5" - 2);
console.log("5" * "2");
console.log(true + true);
console.log(1 + "1");
console.log("abc" < "abd");
console.log(10 % 3);

console.log(2 ** 10);
console.log(2 ** 3 ** 2);

class Vehicle {}
class Car extends Vehicle {}
let car = new Car();
console.log(car instanceof Car);
console.log(car instanceof Vehicle);
console.log(car instanceof Array);
console.log([] instanceof Array);

let obj3 = { a: 1 };
console.log("a" in obj3);
console.log("b" in obj3);
console.log(0 in [1, 2]);

console.log(null ?? "default");
console.log(undefined ?? "default");
console.log(0 ?? "default");

let nested3 = { inner: { val: 42 } };
console.log(nested3?.inner?.val);
console.log(nested3?.missing?.val);
let nothing3 = null;
console.log(nothing3?.foo);
console.log(nothing3?.foo());

let obj4 = { count: 0 };
obj4.count++;
console.log(obj4.count);
++obj4.count;
console.log(obj4.count);
obj4.count += 10;
console.log(obj4.count);

let arr4 = [1, 2, 3];
arr4[0]++;
console.log(arr4[0]);

class Widget {
  constructor() {
    console.log(new.target === Widget);
  }
}
new Widget();

let zeroValue = 0;
zeroValue ||= 5;
console.log(zeroValue);

let nonZeroValue = 3;
nonZeroValue ||= 5;
console.log(nonZeroValue);

let truthyValue = 1;
truthyValue &&= 10;
console.log(truthyValue);

let falsyValue = 0;
falsyValue &&= 10;
console.log(falsyValue);

let nullValue = null;
nullValue ??= 7;
console.log(nullValue);

let definedZero = 0;
definedZero ??= 7;
console.log(definedZero);

let account = { balance: 0, limit: 3, note: null };
account.balance ||= 5;
account.limit ||= 5;
account.note ??= "n/a";
console.log(account.balance, account.limit, account.note);

let readings = [0, 1, null];
readings[0] ||= 42;
readings[1] &&= 42;
readings[2] ??= 42;
console.log(readings[0], readings[1], readings[2]);

function bump(n) {
  return n + 1;
}
let guardedTruthy = 1;
guardedTruthy &&= bump(guardedTruthy);
console.log(guardedTruthy);
let guardedFalsy = 0;
guardedFalsy &&= bump(guardedFalsy);
console.log(guardedFalsy);
