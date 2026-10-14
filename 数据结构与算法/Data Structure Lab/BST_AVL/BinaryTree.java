package BST_AVL;

public interface BinaryTree {
    boolean isEmpty();
    void insert(int data);
    void delete(int data);
    boolean search(int data);
    double ASL();
    void inorderTraversal();
}
