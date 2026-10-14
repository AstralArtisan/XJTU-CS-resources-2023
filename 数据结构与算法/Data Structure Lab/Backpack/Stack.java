package Backpack;

public class Stack {
    private int size;
    private Node top;


    private static class Node {
        int data;
        Node next;

        public Node(int data, Node next) {
            this.data = data;
            this.next = next;
        }
    }

    public Stack() {
        top = new Node(0, null);
        size = 0;
    }

    public void push(int item) {
        Node newNode = new Node(item, null);
        if(!isEmpty()) {
            newNode.next = top;
        }
        top = newNode;
        size++;
    }

    public int pop() {
        if (isEmpty()) {
            throw new IllegalStateException("Stack is empty");
        }
        int value = top.data;
        top = top.next;
        size--;
        return value;
    }

    public boolean isEmpty() {
        return top == null;
    }

    public int peek() {
        if (isEmpty()) {
            throw new IllegalStateException("Stack is empty");
        }
        return top.data;
    }

    public void clear() {
        top = null;
    }

    public int[] toArray() {
        int size = size();
        int[] result = new int[size];
        Node current = top;
        for (int i = size - 1; i >= 0; i--) {
            result[i] = current.data;
            current = current.next;
        }
        return result;
    }

    public int size() {
        return size;
    }
}
