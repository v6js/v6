const traceEvents = require("trace_events");

try {
  traceEvents.createTracing({});
  console.log("should not reach");
} catch (e) {
  console.log("createTracing correctly unsupported");
}
