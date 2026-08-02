const assert = require("assert");

assert(true);
assert.equal(1, "1");
assert.strictEqual(1, 1);
assert.notStrictEqual(1, "1");
assert.deepEqual({ a: 1, b: [1, 2] }, { a: 1, b: [1, 2] });
assert.notDeepEqual({ a: 1 }, { a: 2 });

try {
  assert.strictEqual(1, "1");
  console.log("should not reach");
} catch (e) {
  console.log("caught strictEqual mismatch");
}

try {
  assert(false, "custom message");
  console.log("should not reach");
} catch (e) {
  console.log("caught:", e);
}

assert.throws(() => {
  throw new Error("x");
});
console.log("throws ok");

let threw = false;
try {
  assert.doesNotThrow(() => {
    throw new Error("y");
  });
} catch (e) {
  threw = true;
}
console.log("doesNotThrow caught unexpected throw:", threw);

console.log("assert module ok");
