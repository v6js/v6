class Temperature {
  #celsius = 0;

  get celsius() {
    return this.#celsius;
  }

  set celsius(v) {
    this.#celsius = v;
  }

  get fahrenheit() {
    return this.#celsius * 1.8 + 32;
  }

  set fahrenheit(v) {
    this.#celsius = (v - 32) / 1.8;
  }
}

function run() {
  const t = new Temperature();
  let total = 0;

  for (let i = 0; i < 100000; i++) {
    t.celsius = i % 100;
    total += t.fahrenheit;
    t.fahrenheit = i % 200;
    total += t.celsius;
  }

  return total;
}

console.log(run());
