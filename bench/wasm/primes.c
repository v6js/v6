typedef unsigned int u32;

__attribute__((import_module("wasi_snapshot_preview1"), import_name("fd_write")))
int fd_write(int fd, const void *iovs, int iovs_len, u32 *nwritten);

__attribute__((import_module("wasi_snapshot_preview1"), import_name("proc_exit")))
void proc_exit(int code);

typedef struct { const char *buf; u32 len; } iovec;

#define N 3000000
static unsigned char sieve[N + 1];

__attribute__((export_name("run")))
int run(void) {
  for (int i = 2; i <= N; i++)
    sieve[i] = 1;
  for (int i = 2; i * i <= N; i++) {
    if (sieve[i]) {
      for (int j = i * i; j <= N; j += i)
        sieve[j] = 0;
    }
  }
  int count = 0;
  for (int i = 2; i <= N; i++)
    if (sieve[i])
      count++;
  return count;
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
