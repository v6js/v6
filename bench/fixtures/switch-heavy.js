function classify(n) {
  switch (n % 10) {
    case 0:
      return "zero";
    case 1:
      return "one";
    case 2:
      return "two";
    case 3:
      return "three";
    case 4:
      return "four";
    case 5:
      return "five";
    case 6:
      return "six";
    case 7:
      return "seven";
    case 8:
      return "eight";
    default:
      return "nine";
  }
}

function run() {
  let total = 0;
  for (let i = 0; i < 300000; i++) {
    const label = classify(i);
    total += label.length;
  }
  return total;
}

console.log(run());
