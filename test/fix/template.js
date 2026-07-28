var name = "world";
print(`hello ${name}`);
print(`1 + 2 = ${1 + 2}`);

var x = 3, y = 4;
print(`sum=${x + y} product=${x * y}`);

print(`no interpolation here`);
print(``);

function greet(who) {
  return `hi, ${who}!`;
}
print(greet("Ada"));

var obj = { a: 1 };
print(`obj.a is ${obj.a}`);

print(`nested: ${1 + (2 * 3)}`);
