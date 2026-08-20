typedef unsigned int u32;

__attribute__((import_module("wasi_snapshot_preview1"), import_name("fd_write")))
int fd_write(int fd, const void *iovs, int iovs_len, u32 *nwritten);

__attribute__((import_module("wasi_snapshot_preview1"), import_name("proc_exit")))
void proc_exit(int code);

typedef struct { const char *buf; u32 len; } iovec;

#define TABLE_SIZE 16
#define CALLS 4000000

typedef int (*fnptr)(int);

static int op0(int x) { return x + 1; }
static int op1(int x) { return x + 2; }
static int op2(int x) { return x + 3; }
static int op3(int x) { return x + 4; }
static int op4(int x) { return x + 5; }
static int op5(int x) { return x + 6; }
static int op6(int x) { return x + 7; }
static int op7(int x) { return x + 8; }
static int op8(int x) { return x + 9; }
static int op9(int x) { return x + 10; }
static int op10(int x) { return x + 11; }
static int op11(int x) { return x + 12; }
static int op12(int x) { return x + 13; }
static int op13(int x) { return x + 14; }
static int op14(int x) { return x + 15; }
static int op15(int x) { return x + 16; }

static fnptr table[TABLE_SIZE] = {op0,  op1,  op2,  op3,  op4,  op5,
                                  op6,  op7,  op8,  op9,  op10, op11,
                                  op12, op13, op14, op15};

__attribute__((export_name("run")))
int run(void) {
  int acc = 0;
  for (int i = 0; i < CALLS; i++) {
    fnptr f = table[i & (TABLE_SIZE - 1)];
    acc = f(acc);
  }
  return acc;
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
