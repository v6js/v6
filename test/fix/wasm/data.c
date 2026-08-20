static volatile int buf[1] = {42};

__attribute__((export_name("run")))
int run(void) {
  return buf[0];
}
