function f(x) {
  return +x;
}
function g(x) {
  return -x;
}
function h(x) {
  return +x;
}
function k(x) {
  return +x;
}
console.log(f(5), g(5), h(5), k(5));
function boolctx(x) {
  if (x) {
    return "yes";
  }
  return "no";
}
console.log(boolctx(1), boolctx(0));
function notchain(x) {
  return !x;
}
console.log(notchain(1), notchain(0));
