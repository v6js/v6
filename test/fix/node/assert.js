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

assert.match("hello world", /world/);
try {
  assert.match("hello", /world/);
  console.log("should not reach");
} catch (e) {
  console.log("caught match failure");
}
assert.doesNotMatch("hello", /world/);
console.log("match/doesNotMatch ok");

const { CallTracker } = assert;
const tracker = new CallTracker();
const trackedFn = tracker.calls(() => {}, 2);
trackedFn();
trackedFn();
tracker.verify();
console.log("CallTracker verify passed");

const tracker2 = new CallTracker();
const trackedFn2 = tracker2.calls(() => {}, 2);
trackedFn2();
try {
  tracker2.verify();
  console.log("should not reach");
} catch (e) {
  console.log("CallTracker verify correctly failed");
}
console.log("CallTracker report length:", tracker2.report().length);

async function willReject() {
  throw new TypeError("bad thing");
}
async function willResolve() {
  return 42;
}

Promise.all([
  assert.rejects(willReject(), TypeError).then(() => "rejects TypeError ok"),
  assert.rejects(willReject(), /bad/).then(() => "rejects regex ok"),
  assert.doesNotReject(willResolve()).then(() => "doesNotReject ok"),
]).then((results) => {
  results.forEach((r) => console.log(r));
});
