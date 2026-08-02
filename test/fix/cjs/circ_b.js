const a = require("./circ_a.js");
console.log("circ_b loaded, a.aValue seen at load time:", a.aValue);
exports.bValue = "B";
exports.getFromB = function () {
  return "B sees a=" + a.aValue;
};
