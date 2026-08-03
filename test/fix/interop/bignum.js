import BigInteger from "java:java.math.BigInteger";
import BigDecimal from "java:java.math.BigDecimal";

const a = new BigInteger("123456789012345678901234567890");
console.log(a.toString());
console.log(typeof a);

const doubled = a * 2n;
console.log(doubled.toString());

const fromStatic = BigInteger.valueOf(2);
console.log(fromStatic.toString(), typeof fromStatic);

const jsBig = 999999999999999999999n;
const combined = a + jsBig;
console.log(combined.toString());

const price = new BigDecimal("19.99");
const qty = new BigDecimal("3");
const total = price.multiply(qty);
console.log(total.toString());
console.log(total.compareTo(new BigDecimal("59.97")));
