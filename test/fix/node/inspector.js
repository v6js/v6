const inspector = require("inspector");

try {
  inspector.open();
  console.log("should not reach");
} catch (e) {
  console.log("inspector.open correctly unsupported");
}

try {
  new inspector.Session();
  console.log("should not reach");
} catch (e) {
  console.log("inspector.Session correctly unsupported");
}
