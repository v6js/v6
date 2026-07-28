try {
  throw "boom";
} catch (e) {
  console.log("caught: " + e);
}

function risky(x) {
  if (x < 0) throw "negative";
  return x * 2;
}

try {
  console.log(risky(5));
  console.log(risky(-1));
  console.log("not reached");
} catch (e) {
  console.log("error: " + e);
}

var log = [];
function withFinally() {
  try {
    log[log.length] = "try";
    throw "oops";
  } catch (e) {
    log[log.length] = "catch:" + e;
  } finally {
    log[log.length] = "finally";
  }
}
withFinally();
console.log(log.length);
console.log(log[0]);
console.log(log[1]);
console.log(log[2]);

try {
  try {
    throw "inner";
  } finally {
    console.log("inner finally");
  }
} catch (e) {
  console.log("outer caught: " + e);
}

try {
  console.log("start");
} finally {
  console.log("always runs");
}
