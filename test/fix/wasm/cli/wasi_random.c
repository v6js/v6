typedef unsigned int u32;

__attribute__((import_module("wasi_snapshot_preview1"), import_name("random_get")))
int random_get(void *buf, u32 buf_len);

__attribute__((import_module("wasi_snapshot_preview1"), import_name("fd_write")))
int fd_write(int fd, const void *iovs, int iovs_len, u32 *nwritten);

__attribute__((import_module("wasi_snapshot_preview1"), import_name("proc_exit")))
void proc_exit(int code);

typedef struct { const char *buf; u32 len; } iovec;

static volatile unsigned char randbuf[4];
static const char msg[] = "got random\n";
static iovec iov;
static u32 written;

__attribute__((export_name("_start")))
void _start(void) {
  random_get((void *)randbuf, 4);
  iov.buf = msg;
  iov.len = sizeof(msg) - 1;
  fd_write(1, &iov, 1, &written);
  proc_exit(0);
}
