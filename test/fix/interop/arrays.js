import Base64 from "java:java.util.Base64";
import MessageDigest from "java:java.security.MessageDigest";

const encoder = Base64.getEncoder();
const decoder = Base64.getDecoder();

const bytes = [72, 101, 108, 108, 111];
const encoded = encoder.encodeToString(bytes);
console.log(encoded);

const decoded = decoder.decode(encoded);
console.log(decoded.length);
console.log(decoded[0], decoded[1], decoded[2], decoded[3], decoded[4]);

const md = MessageDigest.getInstance("SHA-256");
const hash = md.digest(bytes);
console.log(hash.length);
console.log(typeof hash[0]);
