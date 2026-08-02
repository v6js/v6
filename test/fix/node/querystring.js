const qs = require("querystring");

console.log(qs.stringify({ a: "1 2", b: ["x", "y"] }));
console.log(JSON.stringify(qs.parse("a=1%202&b=x&b=y")));
console.log(qs.escape("a b/c"));
console.log(qs.unescape("a%20b%2Fc"));
console.log(typeof qs.parse(""));
console.log(qs.stringify({}));
