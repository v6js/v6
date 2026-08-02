const dns = require("dns");

dns.lookup("localhost", (err, address, family) => {
  console.log("lookup err:", err);
  console.log("lookup family:", family === 4 || family === 6);
  console.log("lookup address present:", typeof address === "string" && address.length > 0);
});

dns.promises.lookup("localhost").then((result) => {
  console.log("promise lookup:", typeof result.address, typeof result.family);
});
