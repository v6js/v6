const PI = 3.14159;
const NAME = "world";
const ENABLED = true;
const NOTHING = null;
const UNDEF_VAL = undefined;

function area(r) {
  return PI * r * r;
}

console.log("hello " + NAME);
console.log(area(2));
console.log(ENABLED ? "on" : "off");
console.log(NOTHING === null);
console.log(UNDEF_VAL === undefined);

function outer() {
  const FACTOR = 2;
  return [1, 2, 3].map(function (x) {
    return x * FACTOR;
  });
}
console.log(outer());

let PI2 = 1;
PI2 = 2;
console.log(PI2);

{
  const PI = 99;
  console.log(PI);
}

function shadow() {
  const PI = 100;
  return PI;
}
console.log(shadow());
