package Josephus;

import java.io.File;
import java.io.IOException;
import java.util.Scanner;

public class Josephus_v2 {
    private class Node {
        int password;
        Node next;

        public Node(int password) {
            this.password = password;
        }
    }

    private final int N;
    private int m;
    private Node first;
    private int[] result;

    public Josephus_v2(int N, int m, int[] passwords) {
        this.N = N;
        this.m = m;
        this.result = new int[N];
        this.first = new Node(passwords[0]);

        Node current = first;
        for (int i = 1; i < N; i++) {
            current.next = new Node(passwords[i]);
            current = current.next;
        }
        current.next = first;
    }

    public void solveIteratively() {
        Node current = first;
        Node prev = null;
        int n = N;
        int M = m;
        for (int i = 0; i < N; i++) {
            int count = (M - 1) % n;
            while (count > 0) {
                prev = current;
                current = current.next;
                count--;
            }

            result[i] = current.password;
            M = current.password;

            if (current == current.next) {
                break;
            }
            if (prev != null) {
                prev.next = current.next;
            }
            current = current.next;
            n--;
        }
        printResult("Iteration");
    }

    public void solveRecursively() {
        Count(first, null, m, N, 0);
        printResult("Recursion");
    }

    private void Count(Node current, Node prev, int m, int n, int index) {
        if (n == 0) return;

        int count = (m - 1) % n;
        while (count > 0) {
            prev = current;
            current = current.next;
            count--;
        }

        result[index] = current.password;
        m = current.password;
        if (current == current.next) return;
        prev.next = current.next;
        Count(current.next, prev, m, n - 1, index + 1);
    }

    private void printResult(String method) {
        System.out.print("Answer solved by " + method + ": ");
        for (int i = 0; i < result.length; i++) {
            System.out.print(result[i] + " ");
        }
        System.out.println();
    }

    public static Josephus_v2 readFromFile(String filename) throws IOException {
        Scanner scanner = new Scanner(new File(filename));
        int N = scanner.nextInt();
        int m = scanner.nextInt();
        int[] passwords = new int[N];
        for (int i = 0; i < N; i++) {
            passwords[i] = scanner.nextInt();
        }
        scanner.close();
        return new Josephus_v2(N, m, passwords);
    }

    public static void main(String[] args) {
        try {
            Josephus_v2 josephus1 = readFromFile("JosephusInput.txt");
            Josephus_v2 josephus2 = readFromFile("JosephusInput.txt");
            josephus1.solveIteratively();
            josephus2.solveRecursively();
        } catch (IOException e) {
            System.out.println("Error reading file: " + e.getMessage());
        }
    }
}

