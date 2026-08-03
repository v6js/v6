function* sequence() {
  yield 1;
  yield 2;
  yield 3;
}
const g = sequence();
console.log(g.next().value, g.next().value, g.next().value);
console.log(g.next().done);

function* withReturn() {
  yield "a";
  return "final";
}
const g2 = withReturn();
console.log(g2.next().value, g2.next().value, g2.next().done);

for (const v of sequence()) {
  console.log(v);
}
console.log([...sequence()]);

function* echo() {
  const x = yield "first";
  const y = yield "second";
  return x + y;
}
const g3 = echo();
console.log(g3.next().value);
console.log(g3.next(10).value);
console.log(g3.next(20).value);

function* delegating() {
  yield 1;
  yield* [2, 3, 4];
  yield 5;
}
console.log([...delegating()]);

function* withThrow() {
  try {
    yield 1;
  } catch (e) {
    yield "recovered:" + e;
  }
}
const g4 = withThrow();
console.log(g4.next().value);
console.log(g4.throw("oops").value);

function* withCleanup() {
  try {
    yield 1;
    yield 2;
  } finally {
    console.log("cleanup ran");
  }
}
const g5 = withCleanup();
console.log(g5.next().value);
console.log(g5.return("stopped").value);

class Sequencer {
  *values() {
    yield 1;
    yield 2;
    yield 3;
  }
  static *range(n) {
    for (let i = 0; i < n; i++) {
      yield i;
    }
  }
}
console.log([...new Sequencer().values()]);
console.log([...Sequencer.range(4)]);

const objectWithGenerator = {
  *values() {
    yield "a";
    yield "b";
  },
};
console.log([...objectWithGenerator.values()]);

function counterFrom(start) {
  return (function* () {
    let n = start;
    while (true) {
      yield n;
      n = n + 1;
    }
  })();
}
const counter = counterFrom(10);
console.log(counter.next().value, counter.next().value, counter.next().value);
