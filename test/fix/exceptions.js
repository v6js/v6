try {
  throw "boom";
} catch (e) {
  print("caught: " + e);
}

function risky(x) {
  if (x < 0) throw "negative";
  return x * 2;
}

try {
  print(risky(5));
  print(risky(-1));
  print("not reached");
} catch (e) {
  print("error: " + e);
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
print(log.length);
print(log[0]);
print(log[1]);
print(log[2]);

try {
  try {
    throw "inner";
  } finally {
    print("inner finally");
  }
} catch (e) {
  print("outer caught: " + e);
}

try {
  print("start");
} finally {
  print("always runs");
}
