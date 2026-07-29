let name = "world";
console.log(`hello ${name}`);
console.log(`1 + 2 = ${1 + 2}`);

let x = 3, y = 4;
console.log(`sum=${x + y} product=${x * y}`);

console.log(`no interpolation here`);
console.log(``);

function greet(who) {
  return `hi, ${who}!`;
}
console.log(greet("Ada"));

let obj = { a: 1 };
console.log(`obj.a is ${obj.a}`);

console.log(`nested: ${1 + (2 * 3)}`);
