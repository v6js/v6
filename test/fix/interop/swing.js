import { JFrame, JButton, JLabel, Timer } from "java:javax.swing";
import BorderLayout from "java:java.awt.BorderLayout";

let clicks = 0;

const frame = new JFrame("v6 Swing Demo");
const label = new JLabel("Clicks: 0");
const button = new JButton("Click me");

button.addActionListener((e) => {
  clicks++;
  label.setText("Clicks: " + clicks);
  console.log("button clicked, count =", clicks);
});

frame.getContentPane().add(label, BorderLayout.NORTH);
frame.getContentPane().add(button, BorderLayout.CENTER);
frame.setSize(300, 150);
frame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
frame.setVisible(true);

button.doClick();
button.doClick();
button.doClick();

console.log("final label text:", label.getText());
console.log("frame visible:", frame.isVisible());
console.log("frame title:", frame.getTitle());

// const closer = new Timer(500, (e) => {
//   console.log("closing window");
//   frame.dispose();
//   process.exit(0);
// });
// closer.setRepeats(false);
// closer.start();
