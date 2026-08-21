function withBreak() {
  switch (2) {
    case 1:
      console.log("one");
      break;
    case 2:
      console.log("two");
      break;
    case 3:
      console.log("three");
      break;
    default:
      console.log("other");
  }
}
withBreak();

function withDefault() {
  switch (99) {
    case 1:
      console.log("one");
      break;
    default:
      console.log("default");
  }
}
withDefault();

function noMatchNoDefault() {
  switch ("z") {
    case "a":
      console.log("a");
      break;
  }
  console.log("after");
}
noMatchNoDefault();

function withFallthrough() {
  switch (1) {
    case 1:
      console.log("shared start");
    case 2:
      console.log("shared end");
      break;
    case 3:
      console.log("three");
  }
}
withFallthrough();

function lastCaseNoBreak() {
  switch (2) {
    case 1:
      console.log("one");
      break;
    case 2:
      console.log("two-no-break");
  }
  console.log("done");
}
lastCaseNoBreak();

function labeledBreakInside() {
  outer: for (let i = 0; i < 1; i++) {
    switch (1) {
      case 1:
        console.log("case1");
        break outer;
    }
    console.log("unreached");
  }
  console.log("after loop");
}
labeledBreakInside();

function nonLiteralCase(x) {
  switch (1) {
    case x:
      console.log("matched x");
      break;
    case 1:
      console.log("matched literal");
      break;
  }
}
nonLiteralCase(5);
nonLiteralCase(1);
