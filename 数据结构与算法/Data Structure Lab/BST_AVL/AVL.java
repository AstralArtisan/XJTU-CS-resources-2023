package BST_AVL;

import java.io.*;
import java.util.Scanner;

public class AVL implements BinaryTree{
    private Node root;
    public int insertNum = 0;
    public AVL() {
        root = null;
    }

    private int getHeight(Node node) {
        if (node == null) {
            return 0;
        }
        return node.height;
    }

    private int getBalance(Node node) {
        if (node == null) {
            return 0;
        }
        return getHeight(node.left) - getHeight(node.right);
    }

    /** Right Rotating when LL.*/
    private Node LL(Node root) {
        Node newRoot = root.left;
        root.left = newRoot.right;
        newRoot.right = root;
        root.height = Math.max(getHeight(root.left), getHeight(root.right)) + 1;
        newRoot.height = Math.max(getHeight(newRoot.left), getHeight(newRoot.right)) + 1;
        return newRoot;
    }

    /** Left Rotating when RR.*/
    private Node RR(Node root) {
        Node newRoot = root.right;
        root.right = newRoot.left;
        newRoot.left = root;
        root.height = Math.max(getHeight(root.left), getHeight(root.right)) + 1;
        newRoot.height = Math.max(getHeight(newRoot.left), getHeight(newRoot.right)) + 1;
        return newRoot;
    }

    /** Left-Right Rotating when LR.*/
    private Node LR(Node root) {
        root.left = RR(root.left);
        return LL(root);
    }

    /** Right-Left Rotating when RL.*/
    private Node RL(Node root) {
        root.right = LL(root.right);
        return RR(root);
    }

    private Node rotate(Node curr, int data) {
        int balance = getBalance(curr);
        if (balance > 1 && data < curr.left.data) {
            return LL(curr);
        }
        if (balance < -1 && data > curr.right.data ) {
            return RR(curr);
        }
        if (balance > 1 && data > curr.left.data) {
            return LR(curr);
        }
        if (balance < -1 && data < curr.right.data) {
            return RL(curr);
        }
        return curr;
    }

    @Override
    public boolean isEmpty() {
        return root == null;
    }

    @Override
    public void insert(int data) {
        insertNum++;
        root = insert(root, data);
        try {
            // After each insertion, generate the graph and save it in the specified folder
            String folderPath = "AVL_Trees_Graphs";
            File folder = new File(folderPath);
            if (!folder.exists()) {
                folder.mkdir();  // Create the folder if it does not exist
            }
            String timestamp = String.valueOf(System.currentTimeMillis());  // Unique timestamp for each file
            String dotFilePath = folderPath + File.separator + "AVL_" + insertNum + "_" + timestamp + ".dot";
            generateDotFile(dotFilePath);
            System.out.println("Generated graph for tree after inserting " + data + " at " + dotFilePath);
            String command = "dot -Tpng " + dotFilePath + " -o " + folderPath + File.separator + "AVL_" + insertNum + "_" + timestamp + ".png";
            Runtime.getRuntime().exec(command);  // Generate PNG using Graphviz (make sure Graphviz is installed)
        } catch (IOException e) {
            System.err.println("Error generating graph: " + e.getMessage());
        }
    }
    private Node insert(Node curr, int data) {
        if (curr == null) {
            return new Node(data);
        }
        if (data < curr.data) {
            curr.left = insert(curr.left, data);
        } else if (data > curr.data) {
            curr.right = insert(curr.right, data);
        }
        curr.height = Math.max(getHeight(curr.left), getHeight(curr.right)) + 1;

        curr = rotate(curr, data);
        return curr;
    }

    @Override
    public void delete(int data) {
        root = delete(root, data);
        try {
            String folderPath = "AVL_Trees_Graphs";
            File folder = new File(folderPath);
            if (!folder.exists()) {
                folder.mkdir();  // Create the folder if it does not exist
            }
            String timestamp = String.valueOf(System.currentTimeMillis());  // Unique timestamp for each file
            String dotFilePath = folderPath + File.separator + "AVL_Delete_" + data + "_" + timestamp + ".dot";
            generateDotFile(dotFilePath);
            System.out.println("Generated graph for tree after deleting " + data + " at " + dotFilePath);

            String command = "dot -Tpng " + dotFilePath + " -o " + folderPath + File.separator + "AVL_Delete_" + data + "_" + timestamp + ".png";
            Runtime.getRuntime().exec(command);  // Generate PNG using Graphviz (make sure Graphviz is installed)

        } catch (IOException e) {
            System.err.println("Error generating graph: " + e.getMessage());
        }
    }
    private Node delete(Node curr, int data) {
        if (curr == null) {
            return null;
        }
        if (data < curr.data) {
            curr.left = delete(curr.left, data);
        } else if (data > curr.data) {
            curr.right = delete(curr.right, data);
        } else {
            if (curr.left == null) return curr.right;
            else if (curr.right == null) return curr.left;
            else {
                Node replaced = getReplaced(curr);
                curr.data = replaced.data;
                curr.left = delete(curr.left, replaced.data);
            }
        }
        curr.height = Math.max(getHeight(curr.left), getHeight(curr.right)) + 1;

        curr = rotate(curr, data);
        return curr;
    }
    /** Find the right-most node in the left subtree. */
    private Node getReplaced(Node curr) {
        curr = curr.left;
        while (curr.right != null) {
            curr = curr.right;
        }
        return curr;
    }

    @Override
    public void inorderTraversal() {
        inorderTraversal(root);
        System.out.println();
    }

    private void inorderTraversal(Node curr) {
        if (curr == null) return;
        inorderTraversal(curr.left);
        System.out.print(curr.data + " ");
        inorderTraversal(curr.right);
    }

    @Override
    public double ASL() {
        int[] result = calculateASL(root, 1);
        return (double) result[0] / result[1];
    }

    private int[] calculateASL(Node node, int depth) {
        if (node == null) return new int[]{0, 0};
        int[] left = calculateASL(node.left, depth + 1);
        int[] right = calculateASL(node.right, depth + 1);
        int sum = left[0] + right[0] + depth;
        int count = left[1] + right[1] + 1;
        return new int[]{sum, count};
    }

    @Override
    public boolean search(int data) {
        return search(root, data);
    }

    private boolean search(Node curr, int data) {
        if (curr == null) return false;
        if (data < curr.data) return search(curr.left, data);
        else if (data > curr.data) return search(curr.right, data);
        return true;
    }

    public void generateDotFile(String dotFilePath) throws IOException {
        BufferedWriter writer = new BufferedWriter(new FileWriter(dotFilePath));
        writer.write("digraph G {\n");
        writeGraphNodes(writer, root);
        writer.write("}\n");
        writer.close();
    }

    private void writeGraphNodes(BufferedWriter writer, Node curr) throws IOException {
        if (curr == null) return;
        writer.write("    " + curr.data + " [label=\"" + curr.data + "\"];\n");

        if (curr.left != null) {
            writer.write("    " + curr.data + " -> " + curr.left.data + " [label=\"L\"];\n");
            writeGraphNodes(writer, curr.left);
        }

        if (curr.right != null) {
            writer.write("    " + curr.data + " -> " + curr.right.data + " [label=\"R\"];\n");
            writeGraphNodes(writer, curr.right);
        }
    }

    public static void main(String[] args) throws IOException {
        AVL avl = new AVL();
        try (BufferedReader br = new BufferedReader(new FileReader("AVL.txt"))) {
            String line;
            while ((line = br.readLine()) != null) {
                String[] elements = line.split(" ");
                for (String e : elements) {
                    int val = Integer.parseInt(e);
                    avl.insert(val);
                    System.out.println("After inserting " + val + ":");
                    avl.inorderTraversal();
                    System.out.println("Average Search Length: " + avl.ASL());
                }
            }
        } catch (IOException e) {
            System.err.println("Error reading file: " + e.getMessage());
            return;
        }

        Scanner scanner = new Scanner(System.in);
        System.out.println("Enter element to delete from AVL:");
        int toDelete = scanner.nextInt();
        if (avl.search(toDelete)) {
            avl.delete(toDelete);
            System.out.println("In-order Traversal after Deletion:");
            avl.inorderTraversal();
        } else {
            System.out.println("Can't find:" + toDelete);
        }
    }
}
