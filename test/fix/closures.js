function makeCounter() {
  var count = 0;
  function inc() {
    count = count + 1;
    return count;
  }
  return inc;
}

var c1 = makeCounter();
print(c1());
print(c1());
print(c1());

var c2 = makeCounter();
print(c2());
print(c1());

var add = function(a, b) { return a + b; };
print(add(2, 3));

var mul = (a, b) => a * b;
print(mul(4, 5));

var sq = x => x * x;
print(sq(6));

function apply(f, x) {
  return f(x);
}
print(apply(sq, 7));

var arr = [1, 2, 3];
function sumAll(list) {
  var total = 0;
  for (var i = 0; i < list.length; i = i + 1) {
    total = total + list[i];
  }
  return total;
}
print(sumAll(arr));

print(recFact(5));
function recFact(n) {
  if (n <= 1) return 1;
  return n * recFact(n - 1);
}
