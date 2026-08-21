function unusedLet() {
  let unused = 5;
  console.log("hi");
}

function unusedConstNoInit() {
  console.log("a");
  const usedElsewhere = 1;
  return usedElsewhere;
}

function keepUsedInClosure() {
  let captured = 10;
  function inner() {
    return captured;
  }
  return inner();
}

function keepSideEffectInit() {
  let x = sideEffect();
  console.log("done");
}

function sideEffect() {
  console.log("called");
  return 1;
}

function multiDeclarator() {
  let a = 1, b = 2;
  console.log(b);
}

console.log(unusedLet());
console.log(unusedConstNoInit());
console.log(keepUsedInClosure());
keepSideEffectInit();
multiDeclarator();
