const https = require("https");

try {
  https.createServer(() => {});
  console.log("should not reach");
} catch (e) {
  console.log("createServer throws as documented");
}

console.log(typeof https.request);
console.log(typeof https.get);
