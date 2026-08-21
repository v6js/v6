let obj = { ["a"]: 1, ["b-c"]: 2 };
console.log(obj["a"]);
console.log(obj["b-c"]);
console.log(obj["a"] + obj.a);

class Foo {
  ["method"]() {
    return 1;
  }
  ["1bad"]() {
    return 2;
  }
}
let f = new Foo();
console.log(f.method());
console.log(f["1bad"]());
