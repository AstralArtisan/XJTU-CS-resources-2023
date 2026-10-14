package EightQueens.deque;

public class ArrayDeque<T> implements Deque<T> {
    private T[] items;
    private int size;
    private int nextFirst; //Help the addFirst method as a pointer
    private int nextLast; //Help the addLast method as a pointer

    /** Creates an empty array deque.
      * The array is a circular array.*/
    public ArrayDeque() {
        items = (T[]) new Object[8];
        size = 0;
        nextFirst = 0;
        nextLast = 1;
    }

    private void resize(int capacity) {
        T[] a = (T[]) new Object[capacity];
        int start = (nextFirst + 1) % items.length;
        for (int i = 0; i < size; i++) {
            a[i] = items[(start + i) % items.length];
        }
        items = a;
        nextFirst = capacity - 1;
        nextLast = size;
    }

    /** Adds an item of type T to the front of the deque.
     *  You can assume that item is never null.*/
    public void addFirst(T item) {
        if (size == items.length) {
            resize(size * 2);
        }
        items[nextFirst] = item;
        nextFirst = (nextFirst - 1 + items.length) % items.length;
        size++;
    }

    /** Adds an item of type T to the back of the deque.
     *  You can assume that item is never null.*/
    public void addLast(T item) {
        if (size == items.length) {
            resize(size * 2);
        }
        items[nextLast] = item;
        nextLast = (nextLast + 1 + items.length) % items.length;
        size++;
    }

    /** Returns the number of items in the deque.*/
    public int size() {
        return size;
    }

    /** Prints the items in the deque from first to last, separated by a space.
     *  Once all the items have been printed, print out a new line.*/
    public void printDeque() {
        int index = (nextFirst + 1) % items.length;
        for (int i = 0; i < size; i++) {
            System.out.print(items[index] + " ");
            index = (index + 1) % items.length;
        }
        System.out.println();
    }

    /** Removes and returns the item at the front of the deque.
     *  If no such item exists, returns null.*/
    public T removeFirst() {
        if (isEmpty()) return null;
        if (items.length >= 16 && size < 0.25 * items.length) {
            resize(items.length / 2);
        }
        int First = (nextFirst + 1) % items.length;
        T x = items[First];
        items[First] = null;
        nextFirst = First;
        size--;
        return x;
    }

    /** Removes and returns the item at the back of the deque.
     *  If no such item exists, returns null.*/
    public T removeLast() {
        if (isEmpty()) return null;
        if (items.length >= 16 && size < 0.25 * items.length) {
            resize(items.length / 2);
        }
        int Last = (nextLast - 1 +items.length) % items.length;
        T x = items[Last];
        items[Last] = null;
        nextLast = Last;
        size--;
        return x;
    }

    /** Gets the item at the given index, where 0 is the front, 1 is the next item, and so forth.
     *  If no such item exists, returns null. Must not alter the deque!*/
    public T get(int index) {
        if (isEmpty() || index < 0 || index > size()) return null;
        return items[(nextFirst + index + 1) % items.length];
    }


}
