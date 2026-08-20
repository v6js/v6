typedef unsigned int u32;

__attribute__((import_module("wasi_snapshot_preview1"), import_name("fd_write")))
int fd_write(int fd, const void *iovs, int iovs_len, u32 *nwritten);

__attribute__((import_module("wasi_snapshot_preview1"), import_name("proc_exit")))
void proc_exit(int code);

typedef struct { const char *buf; u32 len; } iovec;

static int fib(int n) {
  return n < 2 ? n : fib(n - 1) + fib(n - 2);
}

__attribute__((export_name("run")))
int run(void) {
  return fib(34);
}

static char outbuf[16];
static iovec iov;
static u32 written;

static char itoa_tmp[12];

static int itoa10(int v, char *buf) {
  if (v == 0) {
    buf[0] = '0';
    return 1;
  }
  char *tmp = itoa_tmp;
  int n = 0;
  while (v > 0) {
    tmp[n++] = (char)('0' + (v % 10));
    v /= 10;
  }
  for (int i = 0; i < n; i++)
    buf[i] = tmp[n - 1 - i];
  return n;
}

__attribute__((export_name("_start")))
void _start(void) {
  int result = run();
  int len = itoa10(result, outbuf);
  outbuf[len] = '\n';
  iov.buf = outbuf;
  iov.len = (u32)(len + 1);
  fd_write(1, &iov, 1, &written);
  proc_exit(0);
}
