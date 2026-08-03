import {
  JFrame,
  JPanel,
  JButton,
  JLabel,
  JTextField,
  JList,
  JScrollPane,
  DefaultListModel,
} from "java:javax.swing";
import BorderLayout from "java:java.awt.BorderLayout";
import FlowLayout from "java:java.awt.FlowLayout";

const frame = new JFrame("v6 Todo List");
frame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);

const model = new DefaultListModel();
const list = new JList(model);
const scrollPane = new JScrollPane(list);

const input = new JTextField(20);
const addButton = new JButton("Add");
const removeButton = new JButton("Remove Selected");
const clearButton = new JButton("Clear All");
const statusLabel = new JLabel("0 items");

function updateStatus() {
  const n = model.getSize();
  statusLabel.setText(n + (n === 1 ? " item" : " items"));
}

function addItem() {
  const text = input.getText().trim();
  if (text.length > 0) {
    model.addElement(text);
    input.setText("");
    updateStatus();
  }
}

addButton.addActionListener((e) => addItem());
input.addActionListener((e) => addItem());

removeButton.addActionListener((e) => {
  const idx = list.getSelectedIndex();
  if (idx >= 0) {
    model.remove(idx);
    updateStatus();
  }
});

clearButton.addActionListener((e) => {
  model.clear();
  updateStatus();
});

const topPanel = new JPanel();
topPanel.setLayout(new FlowLayout());
topPanel.add(input);
topPanel.add(addButton);

const bottomPanel = new JPanel();
bottomPanel.setLayout(new FlowLayout());
bottomPanel.add(removeButton);
bottomPanel.add(clearButton);
bottomPanel.add(statusLabel);

frame.getContentPane().add(topPanel, BorderLayout.NORTH);
frame.getContentPane().add(scrollPane, BorderLayout.CENTER);
frame.getContentPane().add(bottomPanel, BorderLayout.SOUTH);

frame.setSize(420, 320);
frame.setLocationRelativeTo(null);
frame.setVisible(true);

["Write more tests", "Ship the feature", "Fix the build", "Take a break"].forEach(
  (item) => {
    model.addElement(item);
  }
);
updateStatus();

list.setSelectedIndex(1);
removeButton.doClick();

input.setText("Demo item added via doClick");
addButton.doClick();

console.log("item count:", model.getSize());
console.log("status:", statusLabel.getText());
for (let i = 0; i < model.getSize(); i++) {
  console.log(" -", model.getElementAt(i));
}
