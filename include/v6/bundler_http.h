#pragma once

typedef struct v6_bundler_hmr_clients v6_bundler_hmr_clients;

v6_bundler_hmr_clients* v6_bundler_hmr_clients_create(void);
void v6_bundler_hmr_broadcast(v6_bundler_hmr_clients* clients, const char* msg);

int v6_bundler_http_serve(const char* serve_dir, int port,
                          v6_bundler_hmr_clients* clients);
