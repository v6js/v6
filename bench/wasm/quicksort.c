typedef unsigned int u32;

__attribute__((import_module("wasi_snapshot_preview1"), import_name("fd_write")))
int fd_write(int fd, const void *iovs, int iovs_len, u32 *nwritten);

__attribute__((import_module("wasi_snapshot_preview1"), import_name("proc_exit")))
void proc_exit(int code);

typedef struct { const char *buf; u32 len; } iovec;

#define N 20000
#define ITERS 40

static int data[N];
static int scratch[N];

static void fill(unsigned int seed) {
  unsigned int state = seed;
  for (int i = 0; i < N; i++) {
    state = state * 1103515245u + 12345u;
    data[i] = (int)(state >> 8) % 1000000;
  }
}

static void quicksort(int *a, int lo, int hi) {
  while (lo < hi) {
    int pivot = a[(lo + hi) / 2];
    int i = lo, j = hi;
    while (i <= j) {
      while (a[i] < pivot) i++;
      while (a[j] > pivot) j--;
      if (i <= j) {
        int tmp = a[i];
        a[i] = a[j];
        a[j] = tmp;
        i++;
        j--;
      }
    }
    if (lo < j) quicksort(a, lo, j);
    lo = i;
  }
}

__attribute__((export_name("run")))
int run(void) {
  long total = 0;
  for (int iter = 0; iter < ITERS; iter++) {
    fill((unsigned int)(iter * 2654435761u + 1));
    for (int i = 0; i < N; i++)
      scratch[i] = data[i];
    quicksort(scratch, 0, N - 1);
    total += scratch[0] + scratch[N / 2] + scratch[N - 1];
  }
  return (int)(total / ITERS);
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
