#ifndef _WIN32
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#endif

#include "v6/bundle_watch.h"

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define bundle_watch_buf_size 8192

typedef struct watch_dir_entry {
  HANDLE dir_handle;
  OVERLAPPED overlapped;
  char buffer[bundle_watch_buf_size];
} watch_dir_entry;

struct bundle_watcher {
  HANDLE iocp;
  watch_dir_entry* entries;
  int count;
  int cap;
};

static int arm_watch(watch_dir_entry* e) {
  DWORD bytes;
  memset(&e->overlapped, 0, sizeof(e->overlapped));
  BOOL ok = ReadDirectoryChangesW(
      e->dir_handle, e->buffer, bundle_watch_buf_size, FALSE,
      FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE |
          FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_CREATION,
      &bytes, &e->overlapped, NULL);
  return ok ? 0 : -1;
}

bundle_watcher* bundle_watcher_create(void) {
  bundle_watcher* w = calloc(1, sizeof(bundle_watcher));
  w->iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
  return w;
}

int bundle_watcher_add_dir(bundle_watcher* w, const char* dir) {
  if (w->count >= w->cap) {
    w->cap = w->cap == 0 ? 8 : w->cap * 2;
    w->entries = realloc(w->entries, sizeof(watch_dir_entry) * (size_t)w->cap);
  }
  watch_dir_entry* e = &w->entries[w->count];
  memset(e, 0, sizeof(*e));

  HANDLE h = CreateFileA(
      dir, FILE_LIST_DIRECTORY,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
      OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL);
  if (h == INVALID_HANDLE_VALUE)
    return -1;
  e->dir_handle = h;

  if (!CreateIoCompletionPort(h, w->iocp, (ULONG_PTR)(w->count + 1), 0)) {
    CloseHandle(h);
    return -1;
  }
  if (arm_watch(e) != 0) {
    CloseHandle(h);
    return -1;
  }
  w->count++;
  return 0;
}

int bundle_watcher_wait(bundle_watcher* w, int timeout_ms) {
  DWORD bytes;
  ULONG_PTR key;
  LPOVERLAPPED ov;
  BOOL ok = GetQueuedCompletionStatus(
      w->iocp, &bytes, &key, &ov,
      timeout_ms < 0 ? INFINITE : (DWORD)timeout_ms);
  if (!ov)
    return ok ? 1 : 0;
  int idx = (int)key - 1;
  if (idx >= 0 && idx < w->count)
    arm_watch(&w->entries[idx]);
  return 1;
}

void bundle_watcher_free(bundle_watcher* w) {
  if (!w)
    return;
  for (int i = 0; i < w->count; i++)
    CloseHandle(w->entries[i].dir_handle);
  free(w->entries);
  if (w->iocp)
    CloseHandle(w->iocp);
  free(w);
}

#elif defined(__linux__)

#include <sys/epoll.h>
#include <sys/inotify.h>
#include <unistd.h>

struct bundle_watcher {
  int epfd;
  int inotify_fd;
};

bundle_watcher* bundle_watcher_create(void) {
  bundle_watcher* w = calloc(1, sizeof(bundle_watcher));
  w->inotify_fd = inotify_init1(IN_NONBLOCK);
  w->epfd = epoll_create1(0);
  struct epoll_event ev;
  ev.events = EPOLLIN;
  ev.data.fd = w->inotify_fd;
  epoll_ctl(w->epfd, EPOLL_CTL_ADD, w->inotify_fd, &ev);
  return w;
}

int bundle_watcher_add_dir(bundle_watcher* w, const char* dir) {
  int wd = inotify_add_watch(w->inotify_fd, dir,
                             IN_MODIFY | IN_CREATE | IN_DELETE |
                                 IN_MOVED_TO | IN_MOVED_FROM | IN_CLOSE_WRITE);
  return wd >= 0 ? 0 : -1;
}

int bundle_watcher_wait(bundle_watcher* w, int timeout_ms) {
  struct epoll_event ev;
  int n = epoll_wait(w->epfd, &ev, 1, timeout_ms);
  if (n <= 0)
    return n == 0 ? 0 : -1;
  char buf[4096];
  while (read(w->inotify_fd, buf, sizeof(buf)) > 0) {
  }
  return 1;
}

void bundle_watcher_free(bundle_watcher* w) {
  if (!w)
    return;
  close(w->inotify_fd);
  close(w->epfd);
  free(w);
}

#else

#include <fcntl.h>
#include <sys/event.h>
#include <sys/time.h>
#include <unistd.h>

struct bundle_watcher {
  int kq;
  int* fds;
  int count;
  int cap;
};

bundle_watcher* bundle_watcher_create(void) {
  bundle_watcher* w = calloc(1, sizeof(bundle_watcher));
  w->kq = kqueue();
  return w;
}

int bundle_watcher_add_dir(bundle_watcher* w, const char* dir) {
  int fd = open(dir, O_RDONLY);
  if (fd < 0)
    return -1;
  if (w->count >= w->cap) {
    w->cap = w->cap == 0 ? 8 : w->cap * 2;
    w->fds = realloc(w->fds, sizeof(int) * (size_t)w->cap);
  }
  w->fds[w->count++] = fd;

  struct kevent kev;
  EV_SET(&kev, fd, EVFILT_VNODE, EV_ADD | EV_CLEAR,
         NOTE_WRITE | NOTE_EXTEND | NOTE_DELETE | NOTE_RENAME, 0, NULL);
  kevent(w->kq, &kev, 1, NULL, 0, NULL);
  return 0;
}

int bundle_watcher_wait(bundle_watcher* w, int timeout_ms) {
  struct kevent ev;
  struct timespec ts, *tsp = NULL;
  if (timeout_ms >= 0) {
    ts.tv_sec = timeout_ms / 1000;
    ts.tv_nsec = (timeout_ms % 1000) * 1000000;
    tsp = &ts;
  }
  int n = kevent(w->kq, NULL, 0, &ev, 1, tsp);
  return n > 0 ? 1 : (n == 0 ? 0 : -1);
}

void bundle_watcher_free(bundle_watcher* w) {
  if (!w)
    return;
  for (int i = 0; i < w->count; i++)
    close(w->fds[i]);
  close(w->kq);
  free(w->fds);
  free(w);
}

#endif
