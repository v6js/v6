const orig = { a: 1, b: [1, 2, { c: 3 }], d: new Date(2024, 0, 1) };
const cloned = structuredClone(orig);
console.log(cloned.a, JSON.stringify(cloned.b));
console.log(cloned !== orig, cloned.b !== orig.b, cloned.b[2] !== orig.b[2]);
console.log(cloned.d instanceof Date, cloned.d.getTime() === orig.d.getTime());

const cyc = { name: "root" };
cyc.self = cyc;
const clonedCyc = structuredClone(cyc);
console.log(clonedCyc.self === clonedCyc, clonedCyc !== cyc);

const m = new Map([["x", 1], ["y", 2]]);
const clonedMap = structuredClone(m);
console.log(clonedMap instanceof Map, clonedMap !== m, clonedMap.get("x"), clonedMap.get("y"));

const s = new Set([1, 2, 3]);
const clonedSet = structuredClone(s);
console.log(clonedSet instanceof Set, [...clonedSet].join(","));

const shared = { v: 42 };
const withShared = { a: shared, b: shared };
const clonedShared = structuredClone(withShared);
console.log(clonedShared.a === clonedShared.b, clonedShared.a !== shared);

try {
  structuredClone(() => {});
  console.log("should not reach");
} catch (e) {
  console.log("function clone correctly threw");
}

const re = /foo(bar)?/gi;
const clonedRe = structuredClone(re);
console.log(clonedRe instanceof RegExp, clonedRe.test("FOOBAR"), clonedRe.flags);
