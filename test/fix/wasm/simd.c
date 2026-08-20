typedef float v4f32 __attribute__((vector_size(16)));
typedef int v4i32 __attribute__((vector_size(16)));

__attribute__((export_name("f32_ops")))
float f32_ops(float a, float b) {
  v4f32 x = {a, a, a, a};
  v4f32 y = {b, b, b, b};
  v4f32 sum = x + y;
  v4f32 prod = x * y;
  v4f32 diff = x - y;
  v4f32 quot = x / y;
  return sum[0] + prod[1] + diff[2] + quot[3];
}

__attribute__((export_name("i32_ops")))
int i32_ops(int a, int b) {
  v4i32 x = {a, a, a, a};
  v4i32 y = {b, b, b, b};
  v4i32 sum = x + y;
  v4i32 prod = x * y;
  v4i32 diff = x - y;
  return sum[0] + prod[1] + diff[2];
}

__attribute__((export_name("bitwise_ops")))
int bitwise_ops(int a, int b) {
  v4i32 x = {a, a, a, a};
  v4i32 y = {b, b, b, b};
  v4i32 andv = x & y;
  v4i32 orv = x | y;
  v4i32 xorv = x ^ y;
  v4i32 notv = ~x;
  return andv[0] + orv[1] + xorv[2] + notv[3];
}

static float da[4];
static float db[4];

__attribute__((export_name("dot4")))
float dot4(float a0, float a1, float a2, float a3, float b0, float b1,
           float b2, float b3) {
  da[0] = a0;
  da[1] = a1;
  da[2] = a2;
  da[3] = a3;
  db[0] = b0;
  db[1] = b1;
  db[2] = b2;
  db[3] = b3;
  v4f32 *pa = (v4f32 *)da;
  v4f32 *pb = (v4f32 *)db;
  v4f32 p = (*pa) * (*pb);
  return p[0] + p[1] + p[2] + p[3];
}
