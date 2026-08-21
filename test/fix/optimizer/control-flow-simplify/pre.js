function guardClause(x) {
  if (x > 0) {
    console.log("positive");
    return "pos";
  } else {
    console.log("not positive");
    return "nonpos";
  }
}

function nestedGuard(x, y) {
  if (x) {
    if (y) {
      return "both";
    } else {
      return "x only";
    }
  } else {
    return "neither";
  }
}

console.log(guardClause(1));
console.log(guardClause(-1));
console.log(nestedGuard(1, 1));
console.log(nestedGuard(1, 0));
console.log(nestedGuard(0, 0));
