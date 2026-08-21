function shadowed(Number) {
  return Number("5");
}
console.log(shadowed((x) => "shadowed:" + x));
console.log(Number("10"));
