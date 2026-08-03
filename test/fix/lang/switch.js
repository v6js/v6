switch (2) {
  case 1:
    console.log("one");
    break;
  case 2:
    console.log("two");
  case 3:
    console.log("two-or-three fallthrough");
    break;
  default:
    console.log("default");
}

switch (99) {
  case 1:
    console.log("nope");
    break;
  default:
    console.log("default hit");
}

switch ("b") {
  case "a":
    console.log("a");
    break;
  case "b":
    console.log("b");
    break;
}
