const b = require("./circ_b.js");
console.log("circ_a loaded, b.bValue seen at load time:", b.bValue);
exports.aValue = "A";
exports.getFromA = function () {
  return "A sees b=" + b.bValue;
};
