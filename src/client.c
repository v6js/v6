#ifndef _WIN32
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#endif

#include "v6/daemon.h"
#include "v6/buffer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <direct.h>
#include <sys/stat.h>
#define v6_getcwd _getcwd
typedef SOCKET v6_sock;
#define V6_INVALID_SOCK INVALID_SOCKET
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#define v6_getcwd getcwd
typedef int v6_sock;
#define V6_INVALID_SOCK (-1)
extern char** environ;
#endif

static int v6_sockets_init(void) {
#ifdef _WIN32
  WSADATA wsa;
  return WSAStartup(MAKEWORD(2, 2), &wsa) == 0 ? 0 : -1;
#else
  return 0;
#endif
}

static void v6_sock_close(v6_sock s) {
#ifdef _WIN32
  closesocket(s);
#else
  close(s);
#endif
}

static void v6_set_nonblocking(v6_sock s, int nonblock) {
#ifdef _WIN32
  u_long mode = nonblock ? 1 : 0;
  ioctlsocket(s, FIONBIO, &mode);
#else
  int flags = fcntl(s, F_GETFL, 0);
  if (nonblock)
    fcntl(s, F_SETFL, flags | O_NONBLOCK);
  else
    fcntl(s, F_SETFL, flags & ~O_NONBLOCK);
#endif
}

static v6_sock v6_connect_loopback(int port, int timeout_ms) {
  v6_sock s = socket(AF_INET, SOCK_STREAM, 0);
  if (s == V6_INVALID_SOCK)
    return V6_INVALID_SOCK;

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons((unsigned short)port);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

  v6_set_nonblocking(s, 1);
  int rc = connect(s, (struct sockaddr*)&addr, sizeof(addr));
  if (rc != 0) {
    fd_set wfds;
    FD_ZERO(&wfds);
    FD_SET(s, &wfds);
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    rc = select((int)s + 1, NULL, &wfds, NULL, &tv);
    if (rc <= 0) {
      v6_sock_close(s);
      return V6_INVALID_SOCK;
    }
    int err = 0;
#ifdef _WIN32
    int errlen = sizeof(err);
#else
    socklen_t errlen = sizeof(err);
#endif
    getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&err, &errlen);
    if (err != 0) {
      v6_sock_close(s);
      return V6_INVALID_SOCK;
    }
  }
  v6_set_nonblocking(s, 0);
  int one = 1;
  setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char*)&one, sizeof(one));
  return s;
}

static int v6_send_all(v6_sock s, const unsigned char* data, size_t len) {
  size_t sent = 0;
  while (sent < len) {
#ifdef _WIN32
    int n = send(s, (const char*)data + sent, (int)(len - sent), 0);
#else
    ssize_t n = send(s, data + sent, len - sent, 0);
#endif
    if (n <= 0)
      return -1;
    sent += (size_t)n;
  }
  return 0;
}

static int v6_recv_all(v6_sock s, unsigned char* data, size_t len) {
  size_t got = 0;
  while (got < len) {
#ifdef _WIN32
    int n = recv(s, (char*)data + got, (int)(len - got), 0);
#else
    ssize_t n = recv(s, data + got, len - got, 0);
#endif
    if (n <= 0)
      return -1;
    got += (size_t)n;
  }
  return 0;
}

static int v6_recv_u32(v6_sock s, unsigned int* out) {
  unsigned char b[4];
  if (v6_recv_all(s, b, 4) != 0)
    return -1;
  *out = ((unsigned int)b[0] << 24) | ((unsigned int)b[1] << 16) |
         ((unsigned int)b[2] << 8) | (unsigned int)b[3];
  return 0;
}

int v6_get_own_exe_path(char* out, size_t out_size) {
#ifdef _WIN32
  DWORD n = GetModuleFileNameA(NULL, out, (DWORD)out_size);
  return (n == 0 || n >= out_size) ? -1 : 0;
#elif defined(__APPLE__)
  uint32_t size = (uint32_t)out_size;
  return _NSGetExecutablePath(out, &size) == 0 ? 0 : -1;
#else
  ssize_t n = readlink("/proc/self/exe", out, out_size - 1);
  if (n < 0)
    return -1;
  out[n] = '\0';
  return 0;
#endif
}

static unsigned long long v6_hash_str(const char* s) {
  unsigned long long h = 1469598103934665603ULL;
  for (; *s; s++) {
    h ^= (unsigned char)*s;
    h *= 1099511628211ULL;
  }
  return h;
}

static int v6_get_temp_dir(char* out, size_t out_size) {
#ifdef _WIN32
  DWORD n = GetTempPathA((DWORD)out_size, out);
  if (n == 0 || n >= out_size)
    return -1;
  if (n > 0 && out[n - 1] == '\\')
    out[n - 1] = '\0';
  return 0;
#else
  const char* t = getenv("TMPDIR");
  if (!t)
    t = "/tmp";
  snprintf(out, out_size, "%s", t);
  return 0;
#endif
}

static int v6_get_lock_path(const char* exe_path, char* out, size_t out_size) {
  char tmp[1024];
  if (v6_get_temp_dir(tmp, sizeof(tmp)) != 0)
    return -1;
  unsigned long long h = v6_hash_str(exe_path);
  snprintf(out, out_size, "%s/v6-daemon-%016llx.lock", tmp, h);
  return 0;
}

int v6_stat_file(const char* path, long long* mtime, long long* size) {
#ifdef _WIN32
  struct __stat64 st;
  if (_stat64(path, &st) != 0)
    return -1;
  *mtime = (long long)st.st_mtime;
  *size = (long long)st.st_size;
  return 0;
#else
  struct stat st;
  if (stat(path, &st) != 0)
    return -1;
  *mtime = (long long)st.st_mtime;
  *size = (long long)st.st_size;
  return 0;
#endif
}

static int v6_pid_alive(long pid) {
#ifdef _WIN32
  HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
  if (!h)
    return 0;
  DWORD code = 0;
  int alive = GetExitCodeProcess(h, &code) && code == STILL_ACTIVE;
  CloseHandle(h);
  return alive;
#else
  return kill((pid_t)pid, 0) == 0 || errno == EPERM;
#endif
}

typedef struct {
  long pid;
  int port;
  long long mtime;
  long long size;
} v6_lock_info;

static int v6_read_lock_file(const char* path, v6_lock_info* out) {
  FILE* f = fopen(path, "rb");
  if (!f)
    return -1;
  long pid;
  int port;
  long long mtime, size;
  int n = fscanf(f, "%ld %d %lld %lld", &pid, &port, &mtime, &size);
  fclose(f);
  if (n != 4)
    return -1;
  out->pid = pid;
  out->port = port;
  out->mtime = mtime;
  out->size = size;
  return 0;
}

static void v6_spawn_daemon(const char* exe_path, const char* lock_path) {
#ifdef _WIN32
  char cmdline[2048];
  snprintf(cmdline, sizeof(cmdline), "\"%s\" --__v6_daemon_serve__ \"%s\"",
           exe_path, lock_path);
  STARTUPINFOA si;
  PROCESS_INFORMATION pi;
  memset(&si, 0, sizeof(si));
  si.cb = sizeof(si);
  memset(&pi, 0, sizeof(pi));
  if (CreateProcessA(NULL, cmdline, NULL, NULL, FALSE,
                     DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP, NULL, NULL,
                     &si, &pi)) {
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
  }
#else
  pid_t pid = fork();
  if (pid == 0) {
    setsid();
    pid_t pid2 = fork();
    if (pid2 == 0) {
      int devnull = open("/dev/null", O_RDWR);
      if (devnull >= 0) {
        dup2(devnull, 0);
        dup2(devnull, 1);
        dup2(devnull, 2);
        if (devnull > 2)
          close(devnull);
      }
      execl(exe_path, exe_path, "--__v6_daemon_serve__", lock_path,
            (char*)NULL);
      _exit(127);
    }
    _exit(0);
  } else if (pid > 0) {
    int status;
    waitpid(pid, &status, 0);
  }
#endif
}

static int v6_wait_for_lock(const char* lock_path, v6_lock_info* out,
                            int timeout_ms) {
#ifdef _WIN32
  int step_ms = 50;
#else
  struct timespec ts;
  ts.tv_sec = 0;
  ts.tv_nsec = 50 * 1000000L;
  int step_ms = 50;
#endif
  int waited = 0;
  while (waited < timeout_ms) {
    if (v6_read_lock_file(lock_path, out) == 0)
      return 0;
#ifdef _WIN32
    Sleep(step_ms);
#else
    nanosleep(&ts, NULL);
#endif
    waited += step_ms;
  }
  return -1;
}

static void v6_write_env_block(buf* b) {
#ifdef _WIN32
  char* block = GetEnvironmentStringsA();
  if (!block) {
    buf_u32(b, 0);
    return;
  }
  size_t count = 0;
  for (char* p = block; *p; p += strlen(p) + 1)
    count++;
  buf_u32(b, (uint32_t)count);
  for (char* p = block; *p; p += strlen(p) + 1) {
    char* eq = strchr(p, '=');
    if (!eq) {
      buf_u32(b, (uint32_t)strlen(p));
      buf_bytes(b, (const uint8_t*)p, strlen(p));
      buf_u32(b, 0);
      continue;
    }
    size_t klen = (size_t)(eq - p);
    buf_u32(b, (uint32_t)klen);
    buf_bytes(b, (const uint8_t*)p, klen);
    const char* v = eq + 1;
    buf_u32(b, (uint32_t)strlen(v));
    buf_bytes(b, (const uint8_t*)v, strlen(v));
  }
  FreeEnvironmentStringsA(block);
#else
  size_t count = 0;
  for (char** e = environ; *e; e++)
    count++;
  buf_u32(b, (uint32_t)count);
  for (char** e = environ; *e; e++) {
    char* eq = strchr(*e, '=');
    if (!eq) {
      buf_u32(b, (uint32_t)strlen(*e));
      buf_bytes(b, (const uint8_t*)*e, strlen(*e));
      buf_u32(b, 0);
      continue;
    }
    size_t klen = (size_t)(eq - *e);
    buf_u32(b, (uint32_t)klen);
    buf_bytes(b, (const uint8_t*)*e, klen);
    const char* v = eq + 1;
    buf_u32(b, (uint32_t)strlen(v));
    buf_bytes(b, (const uint8_t*)v, strlen(v));
  }
#endif
}

static void v6_build_request(buf* b, v6_daemon_class_entry* classes,
                             int num_classes, const char* script_path,
                             char** script_args, int script_argc,
                             int color_enabled) {
  buf_u32(b, (uint32_t)num_classes);
  for (int i = 0; i < num_classes; i++) {
    size_t nlen = strlen(classes[i].name);
    buf_u32(b, (uint32_t)nlen);
    buf_bytes(b, (const uint8_t*)classes[i].name, nlen);
    buf_u32(b, (uint32_t)classes[i].len);
    buf_bytes(b, classes[i].data, classes[i].len);
  }

  size_t splen = strlen(script_path);
  buf_u32(b, (uint32_t)splen);
  buf_bytes(b, (const uint8_t*)script_path, splen);

  buf_u32(b, (uint32_t)script_argc);
  for (int i = 0; i < script_argc; i++) {
    size_t alen = strlen(script_args[i]);
    buf_u32(b, (uint32_t)alen);
    buf_bytes(b, (const uint8_t*)script_args[i], alen);
  }

  char cwd[1024];
  if (!v6_getcwd(cwd, sizeof(cwd)))
    cwd[0] = '\0';
  size_t cwlen = strlen(cwd);
  buf_u32(b, (uint32_t)cwlen);
  buf_bytes(b, (const uint8_t*)cwd, cwlen);

  v6_write_env_block(b);
  buf_u8(b, (uint8_t)(color_enabled ? 1 : 0));
}

static int v6_stream_response(v6_sock s, int* exit_code) {
  for (;;) {
    unsigned char tag;
#ifdef _WIN32
    int n = recv(s, (char*)&tag, 1, 0);
#else
    ssize_t n = recv(s, &tag, 1, 0);
#endif
    if (n <= 0)
      return -1;

    if (tag == 3) {
      unsigned int code;
      if (v6_recv_u32(s, &code) != 0)
        return -1;
      *exit_code = (int)code;
      return 0;
    }

    unsigned int len;
    if (v6_recv_u32(s, &len) != 0)
      return -1;
    if (len == 0)
      continue;

    unsigned char stackbuf[4096];
    unsigned char* buf_ptr = stackbuf;
    unsigned char* heapbuf = NULL;
    if (len > sizeof(stackbuf)) {
      heapbuf = malloc(len);
      if (!heapbuf)
        return -1;
      buf_ptr = heapbuf;
    }
    if (v6_recv_all(s, buf_ptr, len) != 0) {
      free(heapbuf);
      return -1;
    }
    FILE* dst = tag == 2 ? stderr : stdout;
    fwrite(buf_ptr, 1, len, dst);
    fflush(dst);
    free(heapbuf);
  }
}

int v6_daemon_run(const char* exe_path, v6_daemon_class_entry* classes,
                  int num_classes, const char* script_path, char** script_args,
                  int script_argc, int color_enabled, int* exit_code) {
  if (v6_sockets_init() != 0)
    return 0;

  char resolved_exe[1024];
  const char* use_exe = exe_path;
  if (v6_get_own_exe_path(resolved_exe, sizeof(resolved_exe)) == 0)
    use_exe = resolved_exe;

  char lock_path[1200];
  if (v6_get_lock_path(use_exe, lock_path, sizeof(lock_path)) != 0)
    return 0;

  long long cur_mtime = 0, cur_size = 0;
  v6_stat_file(use_exe, &cur_mtime, &cur_size);

  v6_lock_info info;
  int have_daemon = 0;
  if (v6_read_lock_file(lock_path, &info) == 0 && info.mtime == cur_mtime &&
      info.size == cur_size && v6_pid_alive(info.pid)) {
    have_daemon = 1;
  }

  v6_sock sock = V6_INVALID_SOCK;
  if (have_daemon)
    sock = v6_connect_loopback(info.port, 300);

  if (sock == V6_INVALID_SOCK) {
    v6_spawn_daemon(use_exe, lock_path);
    if (v6_wait_for_lock(lock_path, &info, 4000) != 0)
      return 0;
    sock = v6_connect_loopback(info.port, 1500);
    if (sock == V6_INVALID_SOCK)
      return 0;
  }

  buf req;
  buf_init(&req);
  v6_build_request(&req, classes, num_classes, script_path, script_args,
                   script_argc, color_enabled);

  int ok = v6_send_all(sock, req.data, req.len) == 0;
  buf_free(&req);
  if (!ok) {
    v6_sock_close(sock);
    return 0;
  }

  int rc = v6_stream_response(sock, exit_code);
  v6_sock_close(sock);
  return rc == 0 ? 1 : 0;
}
