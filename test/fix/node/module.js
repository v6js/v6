const mod = require("module");

console.log(Array.isArray(mod.builtinModules));
console.log(mod.isBuiltin("fs"), mod.isBuiltin("not-a-real-module"));

try {
  mod.createRequire("./foo.js");
  console.log("should not reach");
} catch (e) {
  console.log("createRequire correctly unsupported");
}
