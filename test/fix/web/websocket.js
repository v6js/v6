console.log(typeof WebSocket);
console.log(WebSocket.CONNECTING, WebSocket.OPEN, WebSocket.CLOSING, WebSocket.CLOSED);

const ws = new WebSocket("ws://127.0.0.1:1/");
console.log(ws.readyState === WebSocket.CONNECTING);
console.log(typeof ws.send, typeof ws.close, typeof ws.addEventListener);

let sawError = false;
let sawClose = false;
ws.addEventListener("error", () => { sawError = true; });
ws.addEventListener("close", () => {
  sawClose = true;
  console.log("connection-failure handling ok:", sawError, sawClose, ws.readyState === WebSocket.CLOSED);
});
