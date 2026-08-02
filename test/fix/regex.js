let simple = /hello/;
console.log(simple.test("hello world"));
console.log(simple.test("goodbye"));
console.log(typeof simple);
console.log(simple instanceof RegExp);

let ctorForm = new RegExp("wor(l)d", "i");
console.log(ctorForm.test("WORLD"));
console.log(ctorForm.source, ctorForm.flags);

let matched = "hello world".match(/w(o)(r)ld/);
console.log(matched[0], matched[1], matched[2], matched.index);

console.log("2024-01-15".match(/(\d+)-(\d+)-(\d+)/).slice(1).join("/"));

console.log("a1b2c3".replace(/\d/, "#"));
console.log("a1b2c3".replace(/\d/g, "#"));
console.log("a1b2c3".replaceAll(/[a-z]/g, "_"));

console.log("one two  three   four".split(/\s+/));

let counter = /\d+/g;
let source = "10 20 30";
let match;
let collected = [];
while ((match = counter.exec(source)) !== null) {
  collected.push(match[0]);
}
console.log(collected);

console.log(
  [..."a1 b2 c3".matchAll(/([a-z])(\d)/g)].map((m) => m[1] + "-" + m[2])
);

console.log("hello".search(/l+/));
console.log("no match here".search(/xyz/));

console.log(
  "price: $42".replace(/\$(\d+)/, (whole, amount) => "USD " + amount)
);

console.log(/^abc$/.test("abc"));
console.log(/^abc$/.test("xabc"));
console.log(/[^]/.test("\n"));

console.log(/(\d{4})-(\d{2})-(\d{2})/.exec("2024-01-15").slice(1));

console.log(/foo/.toString());
console.log(new RegExp("bar", "gi").toString());

console.log(
  "CamelCase".replace(/([A-Z])/g, (m, c, offset) =>
    (offset > 0 ? "_" : "") + c.toLowerCase()
  )
);

let notRegex = 10 / 2 / 1;
console.log(notRegex);

console.log(new Map() instanceof Map);
console.log(new Set() instanceof Set);
console.log(Promise.resolve(1) instanceof Promise);
