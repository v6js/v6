function add(a, b = 1, ...rest) {
  return a + b + rest.length;
}
class Animal {
  static count = 0;
  #name;
  constructor(name) {
    this.#name = name;
    Animal.count++;
  }
  get name() {
    return this.#name;
  }
  speak() {
    return `${this.#name} says hi`;
  }
}
class Dog extends Animal {
  speak() {
    return super.speak() + "!";
  }
}
const d = new Dog("Rex");
console.log(d.speak());
console.log(Animal.count);
let obj = {a: 1, b: 2, ["c" + "d"]: 3, ...{e: 4}, method() {
  return this.a;
}};
let {a, b: renamed} = obj;
console.log(a, renamed);
let arr = [1, 2, , 4, ...[5, 6]];
let [first, , third, ...tail] = arr;
console.log(first, third, tail);
function* gen() {
  yield 1;
  yield* [2, 3];
  return 4;
}
console.log([...gen()]);
async function delay(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}
async function main() {
  await delay(1);
  try {
    throw new Error("boom");
  } catch (e) {
    console.log("caught: " + e.message);
  } finally {
    console.log("finally");
  }
  let x = 5;
  switch (x) {
    case 1:
      console.log("one");
      break;
    case 5:
      console.log("five");
      break;
    default:
      console.log("other");
  }
  let i = 0;
  outer: for (let j = 0; j < 3; j++) {
    for (let k = 0; k < 3; k++) {
      if (k === 1) {
        continue outer;
      }
      if (j === 2) {
        break outer;
      }
      i++;
    }
  }
  console.log("i=" + i);
  let n = 0;
  for (const key in {p: 1, q: 2}) {
    n++;
  }
  console.log("keys=" + n);
  let sum = 0;
  for (const v of [10, 20, 30]) {
    sum += v;
  }
  console.log("sum=" + sum);
  let m = new Map([["k", "v"]]);
  console.log(m.get("k"));
  let cond = true ? "yes" : "no";
  let nullish = null ?? "fallback";
  let opt = obj?.missing?.deep;
  console.log(cond, nullish, opt);
  let bitwise = 5 & 3 | 2 ^ 1;
  let shifted = 1 << 3;
  let pow = 2 ** 10;
  let neg = -pow;
  let notted = !!0;
  console.log(bitwise, shifted, pow, neg, notted);
  let big = 123456789012345678901234567890n;
  console.log(big + 1n);
  let re = /a[bc]+\/d/gi;
  console.log(re.test("abcd"));
  let template = `line1
line2 ${1 + 1} end`;
  console.log(template);
  let seq = (1, 2, 3);
  console.log(seq);
  let arrow1 = (x) => x * 2;
  let arrow2 = (x, y) => {
    return x + y;
  };
  let arrow3 = () => ({z: 1});
  console.log(arrow1(5), arrow2(2, 3), arrow3().z);
  labelBlock: {
    if (i > 0) {
      break labelBlock;
    }
    console.log("unreached");
  }
  const [{p: pp = 10} = {}] = [{}];
  console.log(pp);
  function withDefault(x = 1 + 1, {y = 2} = {}) {
    return x + y;
  }
  console.log(withDefault());
  let alsoInfinity = 1 / 0;
  let alsoNaN = 0 / 0;
  console.log(alsoInfinity, alsoNaN, -0);
}
main();
