package BST_AVL;

import java.io.*;
import java.util.Scanner;

public class LinkedBST implements BinaryTree {
    private Node root;

    public LinkedBST() {
        root = null;
    }

    @Override
    public boolean isEmpty() {
        return root == null;
    }

    @Override
    public void insert(int data) {
        root = insert(root, data);
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
        return curr;
    }

    @Override
    public void delete(int data) {
        root = delete(root, data);
    }

    private Node delete(Node curr, int data) {
        if (data < curr.data) {
            curr.left = delete(curr.left, data);
        }
        else if (data > curr.data) {
            curr.right = delete(curr.right, data);
        }
        else {
            if (curr.left == null) return curr.right;
            else if (curr.right == null) return curr.left; // Situation that Node has 0 or 1 child, easy to implement.
            else {
                // If Node has 2 children, find the node to replace it first.
                // It can be the right-most node in the left subtree or the left-most node in the right subtree.
                // Use the getReplaced method to find it!
                Node replaced = getReplaced(curr);
                curr.data = replaced.data;
                curr.left = delete(curr.left, replaced.data);
            }
        }
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
        LinkedBST bst = new LinkedBST();
        try (BufferedReader br = new BufferedReader(new FileReader("BST.txt"))) {
            String line;
            while ((line = br.readLine()) != null) {
                String[] elements = line.split(" ");
                for (String e : elements) {
                    int val = Integer.parseInt(e);
                    bst.insert(val);
                    System.out.println("After inserting " + val + ":");
                    bst.inorderTraversal();
                    System.out.println("Average Search Length: " + bst.ASL());
                }
            }
        } catch (IOException e) {
            System.err.println("Error reading file: " + e.getMessage());
            return;
        }

        String dotFilePath = "binary_tree.dot";
        bst.generateDotFile(dotFilePath);

        String command = "dot -Tpng " + dotFilePath + " -o binary_tree.png";

        System.out.println("The BST image is saved in binary_tree.png.");
        Runtime.getRuntime().exec(command);

        Scanner scanner = new Scanner(System.in);
        System.out.println("Enter element to delete from BST:");
        int toDelete = scanner.nextInt();
        if (bst.search(toDelete)) {
            bst.delete(toDelete);
            System.out.println("In-order Traversal after Deletion:");
            bst.inorderTraversal();
        } else {
            System.out.println("Can't find:" + toDelete);
        }
    }
}
