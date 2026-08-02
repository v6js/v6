self.onmessage = (e) => {
  self.postMessage({ echoed: e.data, doubled: e.data.n * 2 });
};
