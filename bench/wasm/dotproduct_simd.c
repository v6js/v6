typedef unsigned int u32;
typedef float v4f32 __attribute__((vector_size(16)));

__attribute__((import_module("wasi_snapshot_preview1"), import_name("fd_write")))
int fd_write(int fd, const void *iovs, int iovs_len, u32 *nwritten);

__attribute__((import_module("wasi_snapshot_preview1"), import_name("proc_exit")))
void proc_exit(int code);

typedef struct { const char *buf; u32 len; } iovec;

#define N 65536
#define ITERS 400

static float a[N];
static float b[N];

__attribute__((export_name("run")))
int run(void) {
  for (int i = 0; i < N; i++) {
    a[i] = (float)(i & 255) * 0.5f;
    b[i] = (float)((i * 7) & 255) * 0.25f;
  }

  v4f32 *va = (v4f32 *)a;
  v4f32 *vb = (v4f32 *)b;
  int lanes = N / 4;

  float total = 0.0f;
  for (int iter = 0; iter < ITERS; iter++) {
    v4f32 acc = {0.0f, 0.0f, 0.0f, 0.0f};
    for (int i = 0; i < lanes; i++)
      acc += va[i] * vb[i];
    total += acc[0] + acc[1] + acc[2] + acc[3];
  }
  return (int)(total / (float)ITERS);
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
