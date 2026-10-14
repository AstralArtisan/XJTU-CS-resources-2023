package BST_AVL;

import java.io.*;
import java.util.ArrayList;
import java.util.Scanner;

public class ArrayBST implements BinaryTree{
    /* If the index of a node is 'i', its parent is (i - 1) / 2,
       its left child is 2 * i + 1, its right child is 2 * i + 2. */
    /* Use an ArrayList to resize the array while inserting. */
    public ArrayList<Integer> arr;
    public int size;
    public ArrayBST() {
        arr = new ArrayList<>();
        size = 0;
    }

    @Override
    public boolean isEmpty() {
        return arr.isEmpty();
    }

    @Override
    public void insert(int data) {
        if (size == 0) arr.add(data);
        else {
            insert(0, data);
        }
        size++;
    }
    private void insert(int index, int data) {
        // Ensure capacity to avoid out-of-bound errors
        if (index >= arr.size()) {
            arr.ensureCapacity(index + 1);
            // Fill in the gap if necessary
            while (arr.size() <= index) {
                arr.add(null);
            }
        }

        if (arr.get(index) == null) {
            arr.set(index, data); // Insert the data at the correct position
        } else {
            if (data < arr.get(index)) {
                insert(index * 2 + 1, data); // Left child
            } else if (data > arr.get(index)) {
                insert(index * 2 + 2, data); // Right child
            }
        }
        shrinkArray();
    }

    @Override
    public void delete(int data) {
        delete(0, data);
    }
    private void delete(int index, int data) {
        while (index < arr.size() && arr.get(index) != null) {
            if (data < arr.get(index)) {
                index = index * 2 + 1; // Move to the left child
            }
            else if (data > arr.get(index)) {
                index = index * 2 + 2; // Move to the right child
            }
            else {
                if (index * 2 + 1 >= arr.size() || arr.get(index * 2 + 1) == null) {
                    if (index * 2 + 2 >= arr.size() || arr.get(index * 2 + 2) == null) {
                        // Is a leaf node
                        arr.set(index, null); // Delete it directly
                    } else {
                        // Only has a right child
                        arr.set(index, arr.get(index * 2 + 2));
                        delete(index * 2 + 2, arr.get(index * 2 + 2));
                    }
                } else if (index * 2 + 2 >= arr.size() || arr.get(index * 2 + 2) == null) {
                    // Only has a left child
                    arr.set(index, arr.get(index * 2 + 1));
                    delete(index * 2 + 1, arr.get(index * 2 + 1));
                } else {
                    // Has both left and right children
                    // Find the right-most node in the left subtree. Create a new getReplaced method.
                    arr.set(index * 2 + 1, getReplaced(index));
                }
                break;
            }
        }
        // Free the memory
        shrinkArray();
    }
    private int getReplaced(int index) {
        while (index * 2 + 2 < arr.size() && arr.get(index * 2 + 2) != null) {
            index = index * 2 + 2;
        }
        int n = arr.get(index);
        delete(index, arr.get(index));
        return n;
    }
    private void shrinkArray() {
        if (arr.size() > size * 2) {
            arr.trimToSize(); // Shrink the ArrayList to its current size
        }
    }

    @Override
    public void inorderTraversal() {
        inorderTraversal(0);
        System.out.println();
        shrinkArray();
    }
    private void inorderTraversal(int index) {
        // Ensure capacity to avoid out-of-bound errors
        if (index >= arr.size()) {
            arr.ensureCapacity(index + 1);
            // Fill in the gap if necessary
            while (arr.size() <= index) {
                arr.add(null);
            }
        }
        if (arr.get(index) == null) return;
        inorderTraversal(index * 2 + 1);
        System.out.print(arr.get(index) + " ");
        inorderTraversal(index * 2 + 2);
    }

    @Override
    public boolean search(int data) {
        return search(0, data);
    }
    private boolean search(int index, int data) {
        if (index > arr.size() || arr.get(index) == null) return false;
        if (data < arr.get(index)) return search(index * 2 + 1, data);
        else if (data > arr.get(index)) return search(index * 2 + 2, data);
        return true;
    }

    @Override
    public double ASL() {
        int[] result = calculateASL(0, 1);
        return (double) result[0] / result[1];
    }
    private int[] calculateASL(int index, int depth) {
        if (index > arr.size() || arr.get(index) == null) {
            return new int[]{0, 0};
        }
        int[] left = calculateASL(index * 2 + 1, depth + 1);
        int[] right = calculateASL(index * 2 + 2, depth + 1);
        int sum = left[0] + right[0] + depth;
        int count = left[1] + right[1] + 1;
        return new int[]{sum, count};
    }

    // New method to generate the Dot file
    public void generateDotFile(String dotFilePath) throws IOException {
        BufferedWriter writer = new BufferedWriter(new FileWriter(dotFilePath));
        writer.write("digraph G {\n");
        writeGraphNodes(writer, 0);
        writer.write("}\n");
        writer.close();
    }

    private void writeGraphNodes(BufferedWriter writer, int index) throws IOException {
        if (index >= arr.size() || arr.get(index) == null) return;
        writer.write("    " + index + " [label=\"" + arr.get(index) + "\"];\n");
        if (index * 2 + 1 < arr.size() && arr.get(index * 2 + 1) != null) {
            writer.write("    " + index + " -> " + (index * 2 + 1) + " [label=\"L\"];\n");
            writeGraphNodes(writer, index * 2 + 1);
        }

        if (index * 2 + 2 < arr.size() && arr.get(index * 2 + 2) != null) {
            writer.write("    " + index + " -> " + (index * 2 + 2) + " [label=\"R\"];\n");
            writeGraphNodes(writer, index * 2 + 2);
        }
    }
    public static void main(String[] args) throws IOException {
        ArrayBST bst = new ArrayBST();

        // Read integers from the "BSTarr.txt" file
        try (BufferedReader br = new BufferedReader(new FileReader("BSTarr.txt"))) {
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

        // Generate Graphviz DOT file for visualization
        String dotFilePath = "binary_tree1.dot";
        bst.generateDotFile(dotFilePath);

        // Convert DOT file to PNG using Graphviz command
        String command = "dot -Tpng " + dotFilePath + " -o binary_tree1.png";
        System.out.println("The BST image is saved in binary_tree1.png.");
        try {
            Runtime.getRuntime().exec(command);
        } catch (IOException e) {
            System.err.println("Error generating Graphviz image: " + e.getMessage());
        }

        // Delete an element from BST as per user input
        Scanner scanner = new Scanner(System.in);
        System.out.println("Enter element to delete from BST:");
        int toDelete = scanner.nextInt();
        if (bst.search(toDelete)) {
            bst.delete(toDelete);
            System.out.println("In-order Traversal after Deletion:");
            bst.inorderTraversal();
        } else {
            System.out.println("Can't find: " + toDelete);
        }
    }
}
