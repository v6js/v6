class Point {
  constructor(x, y) {
    this.x = x;
    this.y = y;
  }

  distanceFromOrigin() {
    return Math.sqrt(this.x * this.x + this.y * this.y);
  }
}

function run() {
  let total = 0;
  for (let i = 0; i < 3000000; i++) {
    const p = new Point(i, i + 1);
    total += p.distanceFromOrigin();
  }
  return total;
}

console.log(run());
