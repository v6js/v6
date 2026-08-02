const http2 = require("http2");

console.log(typeof http2.connect);
console.log(typeof http2.constants);
console.log(http2.constants.HTTP2_HEADER_PATH);
console.log(http2.constants.HTTP2_HEADER_STATUS);

try {
  http2.createServer();
  console.log("should not reach");
} catch (e) {
  console.log("createServer correctly unsupported");
}

try {
  http2.createSecureServer();
  console.log("should not reach");
} catch (e) {
  console.log("createSecureServer correctly unsupported");
}
