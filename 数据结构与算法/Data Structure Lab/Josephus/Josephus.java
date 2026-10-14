package Josephus;

// N=7，七个人的密码依次为3，1，7，2，4，8，4.
// 初始报数上限值m=20。

import java.io.File;
import java.io.IOException;
import java.util.Scanner;

public class Josephus {
    /** Create the basic structure of a Node in the LinkedList. */
    private class Node {
        private int item;
        private Node next;

        public Node(int item, Node next) {
            this.item = item;
            this.next = next;
        }
    }

    public final int N; // Number of people
    public int m; // Initial limit
    public int[] password; //
    public Node first; // The first person's password
    public int[] people; // The order of outing people
    private int peopleIndex = 0; // Total number of the people out

    /** Initialize the problem. */
    public Josephus(int N, int m, int[] password) {
        this.N = N;
        this.m = m;
        this.password = password;
        this.first = new Node(password[0], null);
        this.people = new int[N];
    }

    /** Save the people's password as a circular list. */
    private void initial() {
        Node p = first;
        for (int i = 1; i < N; i++) {
            Node newnode = new Node(password[i], null);
            p.next = newnode;
            p = newnode;
        }
        p.next = first;
    }

    /** Get the password at index. */
    private int get(int index) {
        Node p = first;
        for (int i = 0; i < index; i++) {
            p = p.next;
        }
        return p.item;
    }

    /** Remove the person at index. Save his index, and the total number of people +1. */
    private void remove(int index) {
        Node p = first;
        int num;
        if (index == 0) {
            num = p.item;
            first = first.next;
        }
        else {
            for (int i = 0; i < index - 1; i++) {
                p = p.next;
            }
            num = p.next.item;
            p.next = p.next.next;
        }
        people[peopleIndex++] = num;
    }

    /** Recursively solve the problem. */
    private void count_Recursively(int m, int n, int index, int count) {
        if (count == m) {
            n -= 1;
            m = get(index);
            remove(index);
            index--;
            count = 0;
        }
        if (n == 0) {
            return;
        }
        count_Recursively(m, n, (index + 1) % n, count + 1);
    }

    /** Iteratively solve the problem. */
    private void count_Iteratively(int m, int n) {
        int index = 0;
        for (int i = 0; i < N; i++) {
            index = (index + ((m - 1) % n)) % n;
            m = get(index);
            n = n - 1;
            remove(index);
        }
    }

    /** Print the index number referred to the order of getting out. */
    private void printPassword() {
        for (int i = 0; i < peopleIndex; i++) {
            System.out.print(people[i] + " ");
        }
        System.out.println();
    }

    /** Solve method for users. */
    public void solve_Iteratively() {
        initial();
        count_Iteratively(m, N);
        System.out.print("Answer solved by iteration: ");
        printPassword();
    }

    /** Solve method for users. */
    public void solve_Recursively() {
        initial();
        count_Recursively(m, N, 0, 1);
        System.out.print("Answer solved by recursion: ");
        printPassword();
    }

    public static Josephus readFromFile(String filename) throws IOException {
        File file = new File(filename);
        Scanner scanner = new Scanner(file);
        int N = scanner.nextInt();
        int m = scanner.nextInt();
        int[] password = new int[N];
        for (int i = 0; i < N; i++) {
            password[i] = scanner.nextInt();
        }
        scanner.close();
        return new Josephus(N, m, password);
    }

    public static void main(String[] args) {
        try {
            Josephus josephus = Josephus.readFromFile("JosephusInput.txt");
            josephus.solve_Iteratively();
            //josephus.solve_Recursively();
        } catch (IOException e) {
            System.out.println("读取文件失败: " + e.getMessage());
        }
    }
}
