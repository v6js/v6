class Animal {
  constructor(name) {
    this.name = name;
  }
  speak() {
    return this.name + " makes a sound";
  }
  static kind() {
    return "animal";
  }
}

var a = new Animal("Rex");
console.log(a.speak());
console.log(Animal.kind());

class Dog extends Animal {
  constructor(name, breed) {
    super(name);
    this.breed = breed;
  }
  speak() {
    return super.speak() + " (woof)";
  }
}

var d = new Dog("Fido", "Lab");
console.log(d.speak());
console.log(d.name);
console.log(d.breed);
