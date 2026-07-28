#include "test.h"
#include "v6/parser.h"

static int test_lambda_counter;

static compiler make_compiler(class_file* cf, method* m) {
  compiler c;
  c.cf = cf;
  c.m = m;
  c.parent = NULL;
  c.lambda_counter = &test_lambda_counter;
  c.is_arrow = 0;
  c.param_count = 0;
  c.local_count = 0;
  c.scratch_slot = 1;
  c.next_local_slot = 3;
  c.upvalue_count = 0;
  c.break_depth = 0;
  c.continue_depth = 0;
  c.catch_depth = 0;
  c.brace_depth = 0;
  c.super_name = NULL;
  c.super_len = 0;
  return c;
}

int test_parser(void) {
  int fails = 0;

  class_file cf;
  cf_init(&cf, "Main", "java/lang/Object");
  method* m = cf_method(&cf, acc_public | acc_static, "calc", "()D");
  compiler c = make_compiler(&cf, m);

  parser p;
  parser_init(&p, "1 + 2 * 3");
  int rc = compile_expr(&p, &c);
  v6_check(&fails, rc == 0);
  v6_check(&fails, m->code.len > 0);
  v6_check(&fails, m->code.data[0] == op_new);

  cf_free(&cf);

  class_file cf2;
  cf_init(&cf2, "Main", "java/lang/Object");
  method* m2 = cf_method(&cf2, acc_public | acc_static, "calc", "()D");
  compiler c2 = make_compiler(&cf2, m2);

  parser p2;
  parser_init(&p2, "-(1+2)*4");
  int rc2 = compile_expr(&p2, &c2);
  v6_check(&fails, rc2 == 0);

  cf_free(&cf2);

  class_file cf3;
  cf_init(&cf3, "Main", "java/lang/Object");
  method* m3 = cf_method(&cf3, acc_public | acc_static, "calc", "()D");
  compiler c3 = make_compiler(&cf3, m3);

  parser p3;
  parser_init(&p3, "(1 + 2");
  int rc3 = compile_expr(&p3, &c3);
  v6_check(&fails, rc3 != 0);

  cf_free(&cf3);

  class_file cf4;
  cf_init(&cf4, "Main", "java/lang/Object");
  method* m4 = cf_method(&cf4, acc_public | acc_static, "calc", "()D");
  compiler c4 = make_compiler(&cf4, m4);

  parser p4;
  parser_init(&p4, "true");
  int rc4 = compile_expr(&p4, &c4);
  v6_check(&fails, rc4 == 0);
  v6_check(&fails, m4->code.len > 0);
  v6_check(&fails, m4->code.data[0] == op_new);

  cf_free(&cf4);

  class_file cf5;
  cf_init(&cf5, "Main", "java/lang/Object");
  method* m5 = cf_method(&cf5, acc_public | acc_static, "calc", "()D");
  compiler c5 = make_compiler(&cf5, m5);

  parser p5;
  parser_init(&p5, "\"hi\"");
  int rc5 = compile_expr(&p5, &c5);
  v6_check(&fails, rc5 == 0);

  cf_free(&cf5);

  class_file cf6;
  cf_init(&cf6, "Main", "java/lang/Object");
  method* m6 = cf_method(&cf6, acc_public | acc_static, "calc", "()D");
  compiler c6 = make_compiler(&cf6, m6);

  parser p6;
  parser_init(&p6, "1 < 2");
  int rc6 = compile_expr(&p6, &c6);
  v6_check(&fails, rc6 == 0);

  cf_free(&cf6);

  class_file cf7;
  cf_init(&cf7, "Main", "java/lang/Object");
  method* m7 = cf_method(&cf7, acc_public | acc_static, "calc", "()D");
  compiler c7 = make_compiler(&cf7, m7);

  parser p7;
  parser_init(&p7, "1 == 1");
  int rc7 = compile_expr(&p7, &c7);
  v6_check(&fails, rc7 == 0);

  cf_free(&cf7);

  return fails;
}

int test_compile_program(void) {
  int fails = 0;

  class_file cf;
  cf_init(&cf, "Main", "java/lang/Object");
  compile_result rc = compile_program("print(1 + 2 * 3);", &cf);
  v6_check(&fails, rc.ok);
  v6_check(&fails, cf.method_len == 1);
  cf_free(&cf);

  class_file cf2;
  cf_init(&cf2, "Main", "java/lang/Object");
  compile_result rc2 = compile_program("(1 + 2", &cf2);
  v6_check(&fails, !rc2.ok);
  cf_free(&cf2);

  class_file cf3;
  cf_init(&cf3, "Main", "java/lang/Object");
  compile_result rc3 = compile_program("print(true);", &cf3);
  v6_check(&fails, rc3.ok);
  cf_free(&cf3);

  class_file cf4;
  cf_init(&cf4, "Main", "java/lang/Object");
  compile_result rc4 = compile_program(
      "function add(a, b) { return a + b; } print(add(2, 3));", &cf4);
  v6_check(&fails, rc4.ok);
  v6_check(&fails, cf4.method_len == 2);
  cf_free(&cf4);

  class_file cf5;
  cf_init(&cf5, "Main", "java/lang/Object");
  compile_result rc5 = compile_program("print(nope(1));", &cf5);
  v6_check(&fails, !rc5.ok);
  cf_free(&cf5);

  class_file cf6;
  cf_init(&cf6, "Main", "java/lang/Object");
  compile_result rc6 = compile_program(
      "print(add(2, 3)); function add(a, b) { return a + b; }", &cf6);
  v6_check(&fails, rc6.ok);
  cf_free(&cf6);

  class_file cf7;
  cf_init(&cf7, "Main", "java/lang/Object");
  compile_result rc7 = compile_program("print(1 < 2 ? \"a\" : \"b\");", &cf7);
  v6_check(&fails, rc7.ok);
  cf_free(&cf7);

  class_file cf8;
  cf_init(&cf8, "Main", "java/lang/Object");
  compile_result rc8 =
      compile_program("var x = 1; x += 2; x++; --x; print(x);", &cf8);
  v6_check(&fails, rc8.ok);
  cf_free(&cf8);

  class_file cf9;
  cf_init(&cf9, "Main", "java/lang/Object");
  compile_result rc9 = compile_program(
      "var i = 0; while (i < 3) { if (i == 1) { i++; continue; } print(i); "
      "i++; }",
      &cf9);
  v6_check(&fails, rc9.ok);
  cf_free(&cf9);

  class_file cf10;
  cf_init(&cf10, "Main", "java/lang/Object");
  compile_result rc10 = compile_program(
      "switch (1) { case 1: print(\"a\"); break; default: print(\"b\"); }",
      &cf10);
  v6_check(&fails, rc10.ok);
  cf_free(&cf10);

  class_file cf11;
  cf_init(&cf11, "Main", "java/lang/Object");
  compile_result rc11 = compile_program(
      "var o = { a: 1 }; o.a = 2; print(o.a); print(o[\"a\"]);", &cf11);
  v6_check(&fails, rc11.ok);
  cf_free(&cf11);

  class_file cf12;
  cf_init(&cf12, "Main", "java/lang/Object");
  compile_result rc12 = compile_program(
      "var arr = [1, 2, 3]; print(arr[1]); print(arr.length);", &cf12);
  v6_check(&fails, rc12.ok);
  cf_free(&cf12);

  class_file cf13;
  cf_init(&cf13, "Main", "java/lang/Object");
  compile_result rc13 = compile_program("const x = 1; x = 2;", &cf13);
  v6_check(&fails, !rc13.ok);
  cf_free(&cf13);

  class_file cf14;
  cf_init(&cf14, "Main", "java/lang/Object");
  compile_result rc14 = compile_program("break;", &cf14);
  v6_check(&fails, !rc14.ok);
  cf_free(&cf14);

  class_file cf15;
  cf_init(&cf15, "Main", "java/lang/Object");
  compile_result rc15 = compile_program(
      "var a = 1, b = 2, c = 3; print(a); print(b); print(c);", &cf15);
  v6_check(&fails, rc15.ok);
  cf_free(&cf15);

  class_file cf16;
  cf_init(&cf16, "Main", "java/lang/Object");
  compile_result rc16 =
      compile_program("function add(x, y) { return x + y; }"
                      "var a = add(1, 2), b = 3; print(a); print(b);",
                      &cf16);
  v6_check(&fails, rc16.ok);
  cf_free(&cf16);

  class_file cf17;
  cf_init(&cf17, "Main", "java/lang/Object");
  compile_result rc17 = compile_program(
      "print(typeof 1); print(typeof \"s\"); print(typeof true); "
      "print(typeof undefined); print(typeof null); print(typeof {});",
      &cf17);
  v6_check(&fails, rc17.ok);
  cf_free(&cf17);

  class_file cf18;
  cf_init(&cf18, "Main", "java/lang/Object");
  compile_result rc18 = compile_program(
      "var x = 5; x &= 3; print(x); print(5 | 2); print(5 ^ 1); print(~5); "
      "print(1 << 4); print(256 >> 4); print(-1 >>> 28);",
      &cf18);
  v6_check(&fails, rc18.ok);
  cf_free(&cf18);

  class_file cf19;
  cf_init(&cf19, "Main", "java/lang/Object");
  compile_result rc19 = compile_program(
      "var s = \"hello\"; print(s[0]); print(s[10]); print(s.length);", &cf19);
  v6_check(&fails, rc19.ok);
  cf_free(&cf19);

  class_file cf20;
  cf_init(&cf20, "Main", "java/lang/Object");
  compile_result rc20 = compile_program(
      "var o = { a: 1, b: 2 };"
      "for (var k in o) { print(k); print(o[k]); }"
      "for (let k2 in o) { if (k2 == \"a\") continue; print(k2); }"
      "for (var k3 in o) { if (k3 == \"b\") break; print(k3); }",
      &cf20);
  v6_check(&fails, rc20.ok);
  cf_free(&cf20);

  class_file cf21;
  cf_init(&cf21, "Main", "java/lang/Object");
  compile_result rc21 =
      compile_program("var arr = [1, 2, 3];"
                      "for (var x of arr) { print(x); }"
                      "for (let c of \"ab\") { print(c); }"
                      "for (var y of arr) { if (y == 2) continue; print(y); }"
                      "for (var z of arr) { if (z == 2) break; print(z); }",
                      &cf21);
  v6_check(&fails, rc21.ok);
  cf_free(&cf21);

  class_file cf22;
  cf_init(&cf22, "Main", "java/lang/Object");
  compile_result rc22 = compile_program(
      "print(Math.abs(-5)); print(Math.floor(3.7)); print(Math.ceil(3.2)); "
      "print(Math.sqrt(16)); print(Math.max(3, 7)); print(Math.min(3, 7));",
      &cf22);
  v6_check(&fails, rc22.ok);
  cf_free(&cf22);

  class_file cf23;
  cf_init(&cf23, "Main", "java/lang/Object");
  compile_result rc23 =
      compile_program("function makeCounter() {"
                      "  var count = 0;"
                      "  function inc() { count = count + 1; return count; }"
                      "  return inc;"
                      "}"
                      "var c1 = makeCounter();"
                      "print(c1()); print(c1());",
                      &cf23);
  v6_check(&fails, rc23.ok);
  cf_free(&cf23);

  class_file cf24;
  cf_init(&cf24, "Main", "java/lang/Object");
  compile_result rc24 =
      compile_program("var add = function(a, b) { return a + b; };"
                      "var mul = (a, b) => a * b;"
                      "var sq = x => x * x;"
                      "print(add(2, 3)); print(mul(4, 5)); print(sq(6));",
                      &cf24);
  v6_check(&fails, rc24.ok);
  cf_free(&cf24);

  class_file cf25;
  cf_init(&cf25, "Main", "java/lang/Object");
  compile_result rc25 =
      compile_program("print(recFact(5));"
                      "function recFact(n) { if (n <= 1) return 1; return n * "
                      "recFact(n - 1); }",
                      &cf25);
  v6_check(&fails, rc25.ok);
  cf_free(&cf25);

  class_file cf26;
  cf_init(&cf26, "Main", "java/lang/Object");
  compile_result rc26 =
      compile_program("class Animal {"
                      "  constructor(name) { this.name = name; }"
                      "  speak() { return this.name + \" speaks\"; }"
                      "  static kind() { return \"animal\"; }"
                      "}"
                      "class Dog extends Animal {"
                      "  constructor(name) { super(name); }"
                      "  speak() { return super.speak() + \" (woof)\"; }"
                      "}"
                      "var d = new Dog(\"Fido\");"
                      "print(d.speak()); print(Animal.kind());",
                      &cf26);
  v6_check(&fails, rc26.ok);
  cf_free(&cf26);

  class_file cf27;
  cf_init(&cf27, "Main", "java/lang/Object");
  compile_result rc27 = compile_program(
      "try { throw \"boom\"; } catch (e) { print(\"caught: \" + e); }"
      "try { print(\"a\"); } finally { print(\"b\"); }"
      "try {"
      "  try { throw \"x\"; } finally { print(\"inner\"); }"
      "} catch (e) { print(\"outer: \" + e); }",
      &cf27);
  v6_check(&fails, rc27.ok);
  cf_free(&cf27);

  class_file cf28;
  cf_init(&cf28, "Main", "java/lang/Object");
  compile_result rc28 = compile_program("var name = \"world\";"
                                        "print(`hello ${name}`);"
                                        "print(`sum=${1 + 2}`);"
                                        "print(`plain`);",
                                        &cf28);
  v6_check(&fails, rc28.ok);
  cf_free(&cf28);

  class_file cf29;
  cf_init(&cf29, "Main", "java/lang/Object");
  compile_result rc29 = compile_program(
      "var [a, , b, ...rest] = [1, 2, 3, 4, 5];"
      "print(a); print(b); print(rest.length);"
      "var { x, y: y2 = 9 } = { x: 1 };"
      "print(x); print(y2);"
      "function total(...nums) {"
      "  var s = 0;"
      "  for (var i = 0; i < nums.length; i = i + 1) s = s + nums[i];"
      "  return s;"
      "}"
      "print(total(...[1, 2, 3], 4));"
      "var merged = [...[1, 2], 3];"
      "var o = { ...{ a: 1 }, b: 2 };"
      "print(merged.length); print(o.a); print(o.b);",
      &cf29);
  v6_check(&fails, rc29.ok);
  cf_free(&cf29);

  class_file cf30;
  cf_init(&cf30, "Main", "java/lang/Object");
  compile_result rc30 =
      compile_program("var arr = [3, 1, 2];"
                      "print(arr.map(function(x) { return x * 2; })[0]);"
                      "print(arr.sort()[0]);"
                      "print(Object.keys({ a: 1, b: 2 }).length);"
                      "print(Array.isArray(arr));"
                      "print(Array.of(1, 2).length);"
                      "print(btoa(\"hi\"));"
                      "print(atob(btoa(\"hi\")));",
                      &cf30);
  v6_check(&fails, rc30.ok);
  cf_free(&cf30);

  return fails;
}
