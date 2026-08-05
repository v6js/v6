class A {
  tag() {
    return "A";
  }
}

class B extends A {
  tag() {
    return super.tag() + "B";
  }
}

class C extends B {
  tag() {
    return super.tag() + "C";
  }
}

class D extends C {
  tag() {
    return super.tag() + "D";
  }
}

function run() {
  const instances = [];
  for (let i = 0; i < 20000; i++) {
    instances.push(new D());
  }

  let total = 0;
  for (let i = 0; i < instances.length; i++) {
    total += instances[i].tag().length;
  }
  return total;
}

console.log(run());
