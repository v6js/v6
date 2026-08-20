typedef unsigned int u32;

__attribute__((import_module("wasi_snapshot_preview1"), import_name("fd_write")))
int fd_write(int fd, const void *iovs, int iovs_len, u32 *nwritten);

__attribute__((import_module("wasi_snapshot_preview1"), import_name("proc_exit")))
void proc_exit(int code);

typedef struct { const char *buf; u32 len; } iovec;

static const char msg[] = "hello from wasi\n";
static iovec iov;
static u32 written;

__attribute__((export_name("_start")))
void _start(void) {
  iov.buf = msg;
  iov.len = sizeof(msg) - 1;
  fd_write(1, &iov, 1, &written);
  proc_exit(0);
}
