const dc = require("diagnostics_channel");

const ch = dc.channel("test-channel");
console.log(ch.hasSubscribers);

let received = null;
const listener = (msg, name) => { received = { msg, name }; };
ch.subscribe(listener);
console.log(ch.hasSubscribers);

ch.publish({ hello: "world" });
console.log(JSON.stringify(received));

ch.unsubscribe(listener);
console.log(ch.hasSubscribers);
console.log(dc.channel("test-channel") === ch);
