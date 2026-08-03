console.log(JSON.stringify({ a: 1, b: "two", c: true, d: null }));
console.log(JSON.stringify([1, 2, 3]));
console.log(JSON.stringify("hello \"world\"\n"));
console.log(JSON.stringify(42));
console.log(JSON.stringify(true));
console.log(JSON.stringify(null));
console.log(JSON.stringify(undefined));
console.log(JSON.stringify({ a: undefined, b: 1 }));
console.log(JSON.stringify([undefined, 1, function () {}]));

let nested = { name: "Ada", tags: ["math", "cs"], meta: { active: true, count: 3 } };
console.log(JSON.stringify(nested));
console.log(JSON.stringify(nested, null, 2));

let parsed = JSON.parse('{"x":1,"y":[1,2,3],"z":{"nested":true},"s":"hi\\nthere"}');
console.log(parsed.x, parsed.y.length, parsed.y[2], parsed.z.nested, parsed.s);

console.log(JSON.parse("42"));
console.log(JSON.parse("true"));
console.log(JSON.parse("null"));
console.log(JSON.parse('"a string"'));
console.log(JSON.parse("[1,2,3]").length);

let roundtrip = { a: [1, 2, { b: "c" }], d: 3.14, e: -5, f: 1e10 };
console.log(JSON.stringify(JSON.parse(JSON.stringify(roundtrip))));

try {
  JSON.parse("{invalid}");
} catch (e) {
  console.log("parse error caught:", e);
}

try {
  let circular = {};
  circular.self = circular;
  JSON.stringify(circular);
} catch (e) {
  console.log("circular error caught:", e);
}

let withToJSON = {
  value: 42,
  toJSON() {
    return { custom: this.value };
  },
};
console.log(JSON.stringify(withToJSON));

console.log(JSON.stringify({ b: 2, a: 1 }, ["a"]));

console.log(
  JSON.stringify({ a: 1, b: 2 }, (key, value) =>
    typeof value === "number" ? value * 2 : value
  )
);

console.log(
  JSON.parse('{"a":1,"b":2}', (key, value) =>
    typeof value === "number" ? value * 10 : value
  ).a
);

console.log(JSON.stringify(NaN));
console.log(JSON.stringify(Infinity));
console.log(JSON.stringify({ arr: [] }));
console.log(JSON.stringify({}));
console.log(JSON.stringify([]));
