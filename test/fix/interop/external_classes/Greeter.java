public class Greeter {
  public static String greeting = "Hello";
  public String name;

  public Greeter(String name) {
    this.name = name;
  }

  public String greet() {
    return greeting + ", " + name + "!";
  }

  public static int add(int a, int b) {
    return a + b;
  }

  public static void throwIfNegative(int n) {
    if (n < 0)
      throw new IllegalArgumentException("negative: " + n);
  }
}
