const cluster = require("cluster");

console.log(cluster.isPrimary);
console.log(cluster.isMaster);
console.log(cluster.isWorker);
console.log(cluster.worker);
console.log(typeof cluster.workers);
console.log(typeof cluster.fork);
console.log(typeof cluster.disconnect);
console.log(typeof cluster.on);
