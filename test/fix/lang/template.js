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

function summarize(strings, ...values) {
  let result = "";
  for (let i = 0; i < strings.length; i++) {
    result += strings[i];
    if (i < values.length) result += "<" + values[i] + ">";
  }
  return result;
}
let itemCount = 3;
console.log(summarize`You have ${itemCount} items in ${"your cart"}.`);

function rawAccess(strings) {
  return strings.raw[0];
}
console.log(rawAccess`line1\nline2`);

const formatter = {
  prefix: "log",
  format(strings, ...values) {
    return this.prefix + ": " + strings.join("|") + " / " + values.join(",");
  },
};
console.log(formatter.format`a${1}b${2}c`);

function firstChunk(strings) {
  return strings[0];
}
console.log(firstChunk`no substitutions at all`);
