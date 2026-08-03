function makeCounter() {
  let count = 0;
  function inc() {
    count = count + 1;
    return count;
  }
  return inc;
}

let c1 = makeCounter();
console.log(c1());
console.log(c1());
console.log(c1());

let c2 = makeCounter();
console.log(c2());
console.log(c1());

let add = function(a, b) { return a + b; };
console.log(add(2, 3));

let mul = (a, b) => a * b;
console.log(mul(4, 5));

let sq = x => x * x;
console.log(sq(6));

function apply(f, x) {
  return f(x);
}
console.log(apply(sq, 7));

let arr = [1, 2, 3];
function sumAll(list) {
  let total = 0;
  for (let i = 0; i < list.length; i = i + 1) {
    total = total + list[i];
  }
  return total;
}
console.log(sumAll(arr));

console.log(recFact(5));
function recFact(n) {
  if (n <= 1) return 1;
  return n * recFact(n - 1);
}
