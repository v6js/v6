import JLabel from "java:javax.swing.JLabel";
import JButton from "java:javax.swing.JButton";

const label = new JLabel("hello swing");
console.log(label.getText());

const button = new JButton("click me");
button.setText("clicked");
console.log(button.getText());
