static volatile int buf[1];

__attribute__((export_name("run")))
int run(void) {
  buf[0] = 123;
  return buf[0];
}
