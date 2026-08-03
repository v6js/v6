import Point from "java:java.awt.Point";
import Dimension from "java:java.awt.Dimension";
import Color from "java:java.awt.Color";

const p = new Point(3, 4);
console.log(p.x, p.y);
p.x = 10;
p.y = 20;
console.log(p.x, p.y);

const d = new Dimension(100, 50);
console.log(d.width, d.height);
d.width = 200;
console.log(d.width, d.height);

console.log(Color.RED.getRed(), Color.RED.getGreen(), Color.RED.getBlue());
console.log(Color.BLUE.getBlue());
