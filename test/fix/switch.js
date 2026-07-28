switch (2) {
  case 1:
    print("one");
    break;
  case 2:
    print("two");
  case 3:
    print("two-or-three fallthrough");
    break;
  default:
    print("default");
}

switch (99) {
  case 1:
    print("nope");
    break;
  default:
    print("default hit");
}

switch ("b") {
  case "a":
    print("a");
    break;
  case "b":
    print("b");
    break;
}
