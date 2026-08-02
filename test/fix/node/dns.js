const dns = require("dns");

dns.lookup("localhost", (err, address, family) => {
  console.log("lookup err:", err);
  console.log("lookup family:", family === 4 || family === 6);
  console.log("lookup address present:", typeof address === "string" && address.length > 0);
});

dns.promises.lookup("localhost").then((result) => {
  console.log("promise lookup:", typeof result.address, typeof result.family);
});

console.log(typeof dns.resolveMx, typeof dns.resolveTxt, typeof dns.resolveCname,
            typeof dns.resolveNs, typeof dns.reverse, typeof dns.Resolver);

const resolver = new dns.Resolver();
resolver.setServers(["1.1.1.1", "8.8.8.8"]);
console.log("resolver servers:", resolver.getServers());
console.log(typeof resolver.resolve4, typeof resolver.resolveMx, typeof resolver.cancel);
