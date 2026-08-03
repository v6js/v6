const s1 = Symbol("id");
const s2 = Symbol("id");
console.log(typeof s1);
console.log(s1 === s2);
console.log(s1.toString());
console.log(s1.description);
console.log(typeof Symbol.iterator);

const s3 = Symbol.for("shared");
const s4 = Symbol.for("shared");
console.log(s3 === s4);
