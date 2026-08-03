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

let log = [];
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

function returnsThroughFinally() {
  try {
    return 1;
  } finally {
    console.log("returnsThroughFinally: cleanup");
  }
}
console.log(returnsThroughFinally());

function finallyOverridesReturn() {
  try {
    return 1;
  } finally {
    return 2;
  }
}
console.log(finallyOverridesReturn());

function returnInCatchRunsFinally() {
  try {
    throw "boom";
  } catch (e) {
    return "caught:" + e;
  } finally {
    console.log("returnInCatchRunsFinally: cleanup");
  }
}
console.log(returnInCatchRunsFinally());

function nestedFinallyOrder() {
  try {
    try {
      return "inner";
    } finally {
      console.log("nestedFinallyOrder: inner cleanup");
    }
  } finally {
    console.log("nestedFinallyOrder: outer cleanup");
  }
}
console.log(nestedFinallyOrder());

function breakOutOfTryRunsFinally() {
  let trace = [];
  outer: for (let i = 0; i < 3; i++) {
    try {
      trace.push("try" + i);
      if (i === 1) break outer;
    } finally {
      trace.push("finally" + i);
    }
  }
  return trace.join(",");
}
console.log(breakOutOfTryRunsFinally());

function continueOutOfTryRunsFinally() {
  let trace = [];
  for (let i = 0; i < 3; i++) {
    try {
      if (i === 1) continue;
      trace.push("body" + i);
    } finally {
      trace.push("cleanup" + i);
    }
  }
  return trace.join(",");
}
console.log(continueOutOfTryRunsFinally());

function returnValueCapturedBeforeFinallyMutates() {
  let x = 5;
  try {
    return x;
  } finally {
    x = 999;
  }
}
console.log(returnValueCapturedBeforeFinallyMutates());
