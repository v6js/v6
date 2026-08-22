function sumInvariant(arr, a, b) {
  let total = 0;
  for (let i = 0; i < arr.length; i++) {
    const factor = a * b + 1;
    total += arr[i] * factor;
  }
  return total;
}
console.log(sumInvariant([1, 2, 3], 2, 3));

function mutatesReferenced(n) {
  let a = 1;
  let out = 0;
  for (let i = 0; i < n; i++) {
    const x = a + 1;
    out += x;
    a = a + 1;
  }
  return out;
}
console.log(mutatesReferenced(5));

function usesLoopVar(n) {
  let out = 0;
  for (let i = 0; i < n; i++) {
    const x = i * 2;
    out += x;
  }
  return out;
}
console.log(usesLoopVar(4));

function callsFunction(n) {
  let calls = 0;
  function sideEffecting() {
    calls++;
    return 5;
  }
  let out = 0;
  for (let i = 0; i < n; i++) {
    const x = sideEffecting();
    out += x;
  }
  return out + calls;
}
console.log(callsFunction(3));

function memberAccessNotHoisted(obj, n) {
  let out = 0;
  for (let i = 0; i < n; i++) {
    const x = obj.value;
    out += x;
  }
  return out;
}
console.log(memberAccessNotHoisted({ value: 7 }, 3));

function whileLoop(n) {
  let i = 0;
  let out = 0;
  const k = 10;
  while (i < n) {
    const y = k + 1;
    out += y;
    i++;
  }
  return out;
}
console.log(whileLoop(4));

function doWhileLoop(n) {
  let i = 0;
  let out = 0;
  do {
    const z = n * 2;
    out += z;
    i++;
  } while (i < n);
  return out;
}
console.log(doWhileLoop(3));

function nestedShadow() {
  let out = 0;
  for (let i = 0; i < 3; i++) {
    {
      const dup = 5;
      out += dup;
    }
  }
  {
    const dup = 99;
    out += dup;
  }
  return out;
}
console.log(nestedShadow());

function zeroIterations(n) {
  let ran = false;
  for (let i = 0; i < n; i++) {
    const inv = 1 + 1;
    ran = true;
    console.log(inv);
  }
  return ran;
}
console.log(zeroIterations(0));
console.log(zeroIterations(2));
