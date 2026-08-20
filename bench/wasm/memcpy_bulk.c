typedef unsigned int u32;

__attribute__((import_module("wasi_snapshot_preview1"), import_name("fd_write")))
int fd_write(int fd, const void *iovs, int iovs_len, u32 *nwritten);

__attribute__((import_module("wasi_snapshot_preview1"), import_name("proc_exit")))
void proc_exit(int code);

typedef struct { const char *buf; u32 len; } iovec;

#define BUF_LEN (1 << 20)
#define ITERS 400

static unsigned char src[BUF_LEN];
static unsigned char dst[BUF_LEN];

__attribute__((export_name("run")))
int run(void) {
  for (int i = 0; i < BUF_LEN; i++)
    src[i] = (unsigned char)i;

  for (int iter = 0; iter < ITERS; iter++)
    __builtin_memcpy(dst, src, BUF_LEN);

  unsigned int checksum = 0;
  for (int i = 0; i < BUF_LEN; i += 4093)
    checksum += dst[i];
  return (int)checksum;
}

static char outbuf[16];
static iovec iov;
static u32 written;

static int itoa10(int v, char *buf) {
  if (v == 0) {
    buf[0] = '0';
    return 1;
  }
  static char tmp[12];
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
