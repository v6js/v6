const util = require("util");

console.log(util.format("%s is %d years old", "Alice", 30));
console.log(util.format("%j", { a: 1, b: [1, 2, 3] }));
console.log(util.format("extra", 1, 2));
console.log(util.inspect({ a: 1, b: "x", c: [1, 2] }));
console.log(util.inspect("plain string"));

class Base {}
Base.prototype.speak = function () { return "base speak"; };
class Derived {}
util.inherits(Derived, Base);
Derived.prototype.extra = function () { return "derived extra"; };
const d = new Derived();
console.log(d.speak());
console.log(d.extra());

function cbStyle(x, cb) {
  cb(null, x * 2);
}
const promisified = util.promisify(cbStyle);
promisified(21).then((v) => console.log("promisified: " + v));

console.log(util.types.isPromise(Promise.resolve(1)));
console.log(util.types.isRegExp(/a/));
console.log(util.types.isMap(new Map()));
console.log(util.types.isSet(new Set()));
console.log(util.types.isMap("no"));

const dep = util.deprecate(() => 42, "old fn");
console.log(dep());
console.log(dep());

async function doubleAsync(x) { return x * 2; }
const cbf = util.callbackify(doubleAsync);
cbf(5, (err, v) => console.log("callbackify:", err, v));
