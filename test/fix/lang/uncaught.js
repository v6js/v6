function inner() {
  throw new Error("something went wrong");
}

function outer() {
  inner();
}

outer();
