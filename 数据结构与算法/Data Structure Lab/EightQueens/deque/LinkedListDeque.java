package EightQueens.deque;

public class LinkedListDeque<T> implements Deque<T> {

    private class Node {
        private T item;
        private Node next;
        private Node prev;
        private Node(T i, Node p, Node n) {
            item = i;
            next = n;
            prev = p;
        }
    }

    /** The sentinel is always at the front of the list.（带有头结点的列表）
     *  The first item (if it exists) is at sentinel.next.*/
    private Node sentinel;
    private int size;

    /** Creates an empty linked list deque.*/
    public LinkedListDeque() {
        sentinel = new Node(null, null, null);
        sentinel.next = sentinel;
        sentinel.prev = sentinel;
        size = 0;
    }

    /** Adds an item of type T to the front of the deque.
     *  You can assume that item is never null.*/
    public void addFirst(T item) {
        Node first = new Node(item, sentinel, sentinel.next);
        sentinel.next.prev = first;
        sentinel.next = first;
        size++;
    }

    /** Adds an item of type T to the back of the deque.
     *  You can assume that item is never null.*/
    public void addLast(T item) {
        Node last = new Node(item, sentinel.prev, sentinel);
        sentinel.prev.next = last;
        sentinel.prev = last;
        size++;
    }

    /** Returns the number of items in the deque.*/
    public int size() {
        return size;
    }

    /** Prints the items in the deque from first to last, separated by a space.
     *  Once all the items have been printed, print out a new line.*/
    public void printDeque() {
        Node p = sentinel.next;
        while (p != sentinel){
            System.out.print(p.item + " ");
            p = p.next;
        }
        System.out.println();
    }

    /** Removes and returns the item at the front of the deque.
     *  If no such item exists, returns null.*/
    public T removeFirst() {
        if (isEmpty()) return null;
        else {
            T returnNumber = sentinel.next.item;
            sentinel.next.next.prev = sentinel;
            sentinel.next =sentinel.next.next;
            size--;
            return returnNumber;
        }
    }

    /** Removes and returns the item at the back of the deque.
     *  If no such item exists, returns null.*/
    public T removeLast() {
        if (isEmpty()) return null;
        else {
            T returnNumber = sentinel.prev.item;
            sentinel.prev.prev.next = sentinel;
            sentinel.prev = sentinel.prev.prev;
            size--;
            return returnNumber;
        }
    }

    /** Gets the item at the given index, where 0 is the front, 1 is the next item, and so forth.
     *  If no such item exists, returns null. Must not alter the deque!*/
    public T get(int index) {
        if (isEmpty() || index < 0) return null;
        else {
            Node p = sentinel;
            for (int i = 0; i < index; i++) {
                p = p.next;
            }
            return p.next.item;
        }
    }


}
