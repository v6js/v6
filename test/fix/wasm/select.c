__attribute__((export_name("sel")))
int sel(int a, int b, int c) {
  return c ? a : b;
}
