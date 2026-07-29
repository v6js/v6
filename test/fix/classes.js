class Shape {
  constructor(name) {
    this.name = name;
  }
  describe() {
    return this.name + " has no defined area";
  }
  static category() {
    return "shape";
  }
}

let s = new Shape("generic");
console.log(s.describe());
console.log(Shape.category());

class Rectangle extends Shape {
  constructor(width, height) {
    super("rectangle");
    this.width = width;
    this.height = height;
  }
  describe() {
    return super.describe() + " (overridden: " + (this.width * this.height) + ")";
  }
}

let r = new Rectangle(4, 5);
console.log(r.describe());
console.log(r.name);
console.log(r.width);

class Circle {
  constructor(r) {
    this._r = r;
  }
  get area() {
    return Math.round(Math.PI * this._r * this._r * 100) / 100;
  }
  set radius(r) {
    this._r = r;
  }
  get radius() {
    return this._r;
  }
  static get description() {
    return "a circle";
  }
}
let c1 = new Circle(2);
console.log(c1.area);
c1.radius = 3;
console.log(c1.radius);
console.log(c1.area);
console.log(Circle.description);

class Point {
  x = 0;
  y = 0;
  label = "point";
  constructor(x, y) {
    this.x = x;
    this.y = y;
  }
  toString() {
    return this.label + ":" + this.x + "," + this.y;
  }
}
console.log(new Point(3, 4).toString());

class Registry {
  static version = "1.0";
  static count = 0;
  static register() {
    return ++Registry.count;
  }
}
console.log(Registry.version);
console.log(Registry.register());
console.log(Registry.register());

class Settings {
  timeout = 30;
}
console.log(new Settings().timeout);

class Resource {
  id = "resource-init";
}
class ManagedResource extends Resource {
  owner = "owner-init";
}
let mr = new ManagedResource();
console.log(mr.id, mr.owner);

class Entity {
  constructor(id) {
    this.id = id;
  }
}
class Record extends Entity {}
console.log(new Record(42).id);

class Account {
  #balance = 42;
  balance() {
    return this.#balance;
  }
  deposit(v) {
    this.#balance += v;
  }
}
let acc = new Account();
console.log(acc.balance());
acc.deposit(58);
console.log(acc.balance());
