#ifndef _WIN32
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#endif

#include "v6/bundler_http.h"
#include "v6/bundler_strbuf.h"
#include "v6/bundler_thread.h"
#include "v6/bundler_ws.h"
#include "v6/color.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
typedef SOCKET v6_sock;
#define V6_INVALID_SOCK INVALID_SOCKET
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int v6_sock;
#define V6_INVALID_SOCK (-1)
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

static long v6_send_raw(v6_sock s, const void* data, size_t len) {
#ifdef _WIN32
  return send(s, (const char*)data, (int)len, 0);
#else
  return (long)send(s, data, len, 0);
#endif
}

static long v6_recv_raw(v6_sock s, void* data, size_t len) {
#ifdef _WIN32
  return recv(s, (char*)data, (int)len, 0);
#else
  return (long)recv(s, data, len, 0);
#endif
}

struct v6_bundler_hmr_clients {
  v6_bundler_mutex* mu;
  v6_sock* socks;
  int count;
  int cap;
};

v6_bundler_hmr_clients* v6_bundler_hmr_clients_create(void) {
  v6_bundler_hmr_clients* l = malloc(sizeof(v6_bundler_hmr_clients));
  l->mu = v6_bundler_mutex_create();
  l->socks = NULL;
  l->count = 0;
  l->cap = 0;
  return l;
}

static void hmr_list_add(v6_bundler_hmr_clients* l, v6_sock s) {
  v6_bundler_mutex_lock(l->mu);
  if (l->count >= l->cap) {
    l->cap = l->cap == 0 ? 8 : l->cap * 2;
    l->socks = realloc(l->socks, sizeof(v6_sock) * (size_t)l->cap);
  }
  l->socks[l->count++] = s;
  v6_bundler_mutex_unlock(l->mu);
}

void v6_bundler_hmr_broadcast(v6_bundler_hmr_clients* l, const char* msg) {
  size_t msg_len = strlen(msg);
  size_t cap = msg_len + 8;
  unsigned char* frame = malloc(cap);
  size_t frame_len;
  if (v6_bundler_ws_encode_text_frame(msg, msg_len, frame, cap, &frame_len) != 0) {
    free(frame);
    return;
  }
  v6_bundler_mutex_lock(l->mu);
  int write_idx = 0;
  for (int i = 0; i < l->count; i++) {
    long rc = v6_send_raw(l->socks[i], frame, frame_len);
    if (rc == (long)frame_len) {
      l->socks[write_idx++] = l->socks[i];
    } else {
      v6_sock_close(l->socks[i]);
    }
  }
  l->count = write_idx;
  v6_bundler_mutex_unlock(l->mu);
  free(frame);
}

static char* read_whole_file(const char* path, size_t* out_len) {
  FILE* f = fopen(path, "rb");
  if (!f)
    return NULL;
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (n < 0) {
    fclose(f);
    return NULL;
  }
  char* buf = malloc((size_t)n + 1);
  size_t got = fread(buf, 1, (size_t)n, f);
  buf[got] = '\0';
  fclose(f);
  if (out_len)
    *out_len = got;
  return buf;
}

static const char* content_type_for(const char* path) {
  const char* dot = strrchr(path, '.');
  if (!dot)
    return "application/octet-stream";
  if (strcmp(dot, ".html") == 0)
    return "text/html; charset=utf-8";
  if (strcmp(dot, ".js") == 0 || strcmp(dot, ".mjs") == 0)
    return "text/javascript; charset=utf-8";
  if (strcmp(dot, ".css") == 0)
    return "text/css; charset=utf-8";
  if (strcmp(dot, ".json") == 0)
    return "application/json";
  if (strcmp(dot, ".png") == 0)
    return "image/png";
  if (strcmp(dot, ".svg") == 0)
    return "image/svg+xml";
  if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0)
    return "image/jpeg";
  if (strcmp(dot, ".ico") == 0)
    return "image/x-icon";
  return "application/octet-stream";
}

static const char hmr_client_script[] =
    "<script>(function(){var ws=new WebSocket(\"ws://\"+location.host+"
    "\"/__v6_hmr__\");ws.onmessage=function(ev){(0,eval)(ev.data);};})();"
    "</script>";

typedef struct http_request {
  char method[8];
  char path[1024];
  char ws_key[128];
  int is_ws_upgrade;
} http_request;

static int recv_headers(v6_sock s, char* buf, size_t buf_cap, size_t* out_len) {
  size_t len = 0;
  for (;;) {
    if (len + 1 >= buf_cap)
      return -1;
    long n = v6_recv_raw(s, buf + len, buf_cap - 1 - len);
    if (n <= 0)
      return -1;
    len += (size_t)n;
    buf[len] = '\0';
    if (strstr(buf, "\r\n\r\n"))
      break;
  }
  *out_len = len;
  return 0;
}

static void parse_request(const char* buf, http_request* req) {
  memset(req, 0, sizeof(*req));
  sscanf(buf, "%7s %1023s", req->method, req->path);

  const char* upgrade = strstr(buf, "Upgrade:");
  if (!upgrade)
    upgrade = strstr(buf, "upgrade:");
  if (upgrade) {
    const char* line_end = strstr(upgrade, "\r\n");
    size_t seg_len = line_end ? (size_t)(line_end - upgrade) : strlen(upgrade);
    for (size_t i = 0; i < seg_len; i++) {
      if (upgrade[i] == 'w' && strncmp(upgrade + i, "websocket", 9) == 0) {
        req->is_ws_upgrade = 1;
        break;
      }
    }
  }

  const char* key = strstr(buf, "Sec-WebSocket-Key:");
  if (key) {
    key += strlen("Sec-WebSocket-Key:");
    while (*key == ' ')
      key++;
    int i = 0;
    while (*key && *key != '\r' && *key != '\n' && i < (int)sizeof(req->ws_key) - 1)
      req->ws_key[i++] = *key++;
    req->ws_key[i] = '\0';
  }
}

static void serve_file(v6_sock s, const char* full_path) {
  size_t len;
  char* data = read_whole_file(full_path, &len);
  if (!data) {
    const char* resp =
        "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    v6_send_raw(s, resp, strlen(resp));
    return;
  }

  const char* ctype = content_type_for(full_path);
  int is_html = strstr(ctype, "text/html") != NULL;

  v6_bundler_strbuf body;
  v6_bundler_strbuf_init(&body);
  if (is_html) {
    char* body_end = strstr(data, "</body>");
    if (body_end) {
      v6_bundler_strbuf_append(&body, data, (size_t)(body_end - data));
      v6_bundler_strbuf_append_cstr(&body, hmr_client_script);
      v6_bundler_strbuf_append_cstr(&body, body_end);
    } else {
      v6_bundler_strbuf_append(&body, data, len);
      v6_bundler_strbuf_append_cstr(&body, hmr_client_script);
    }
  } else {
    v6_bundler_strbuf_append(&body, data, len);
  }
  free(data);

  char header[256];
  int hn = snprintf(header, sizeof(header),
                    "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: "
                    "%zu\r\nConnection: close\r\n\r\n",
                    ctype, body.len);
  v6_send_raw(s, header, (size_t)hn);
  v6_send_raw(s, body.data, body.len);
  v6_bundler_strbuf_free(&body);
}

typedef struct conn_ctx {
  v6_sock sock;
  char serve_dir[1024];
  v6_bundler_hmr_clients* clients;
} conn_ctx;

static void handle_connection(void* arg) {
  conn_ctx* c = (conn_ctx*)arg;
  char buf[8192];
  size_t len;

  if (recv_headers(c->sock, buf, sizeof(buf), &len) != 0) {
    v6_sock_close(c->sock);
    free(c);
    return;
  }

  http_request req;
  parse_request(buf, &req);

  if (req.is_ws_upgrade) {
    char accept_key[64];
    v6_bundler_ws_accept_key(req.ws_key, accept_key, sizeof(accept_key));
    char resp[512];
    int n = snprintf(resp, sizeof(resp),
                     "HTTP/1.1 101 Switching Protocols\r\nUpgrade: "
                     "websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: "
                     "%s\r\n\r\n",
                     accept_key);
    v6_send_raw(c->sock, resp, (size_t)n);
    hmr_list_add(c->clients, c->sock);
    free(c);
    return;
  }

  char path[1200];
  const char* req_path = req.path[0] ? req.path : "/";
  if (strcmp(req_path, "/") == 0)
    req_path = "/index.html";
  snprintf(path, sizeof(path), "%s%s", c->serve_dir, req_path);
  serve_file(c->sock, path);
  v6_sock_close(c->sock);
  free(c);
}

int v6_bundler_http_serve(const char* serve_dir, int port,
                          v6_bundler_hmr_clients* clients) {
  if (v6_sockets_init() != 0) {
    fprintf(stderr, "error: failed to initialize sockets\n");
    return 1;
  }

  v6_sock listener = socket(AF_INET, SOCK_STREAM, 0);
  int reuse = 1;
  setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons((unsigned short)port);

  if (bind(listener, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
    fprintf(stderr, "error: cannot bind to port %d\n", port);
    return 1;
  }
  listen(listener, 16);

  int c = v6_color_enabled_out();
  printf("%s%s%12s%s http://localhost:%d\n", v6_c_bold(c), v6_c_green(c), "Listening @",
        v6_c_reset(c), port);
  fflush(stdout);

  for (;;) {
    v6_sock client = accept(listener, NULL, NULL);
    if (client == V6_INVALID_SOCK)
      continue;
    conn_ctx* ctx = malloc(sizeof(conn_ctx));
    ctx->sock = client;
    snprintf(ctx->serve_dir, sizeof(ctx->serve_dir), "%s", serve_dir);
    ctx->clients = clients;
    v6_bundler_thread_start(handle_connection, ctx);
  }
}
