function unreachableAfterReturn() {
  console.log("before");
  return 1;
  console.log("dead");
}

function deadIfFalse(x) {
  if (false) {
    console.log("dead branch");
  }
  console.log("after");
  return x;
}

function keepIfTrue(x) {
  if (true) {
    console.log("kept branch");
  } else {
    console.log("dead else");
  }
  return x;
}

function deadWhile() {
  while (false) {
    console.log("never");
  }
  console.log("reached");
}

console.log(unreachableAfterReturn());
console.log(deadIfFalse(5));
console.log(keepIfTrue(6));
deadWhile();
