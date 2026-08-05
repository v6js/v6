function buildCorpus(n) {
  let s = "";
  for (let i = 0; i < n; i++) {
    s += "user" + i + "@example" + (i % 50) + ".com, order-" + i + "-ok; ";
  }
  return s;
}

function run() {
  const corpus = buildCorpus(5000);
  const emailRe = /[a-zA-Z0-9_.+-]+@[a-zA-Z0-9-]+\.[a-zA-Z0-9-.]+/g;
  const orderRe = /order-(\d+)-(\w+)/g;

  const emails = corpus.match(emailRe);
  let total = emails ? emails.length : 0;

  let m;
  let sum = 0;
  while ((m = orderRe.exec(corpus)) !== null) {
    sum += parseInt(m[1], 10);
  }

  return total + sum;
}

console.log(run());
