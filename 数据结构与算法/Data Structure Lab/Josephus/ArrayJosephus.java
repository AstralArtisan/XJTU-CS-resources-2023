package Josephus;

import java.io.File;
import java.io.IOException;
import java.util.Scanner;

public class ArrayJosephus {
    public static void solve(int N, int m, int[] password) {
        int[] people = new int[N];
        for (int i = 0; i < N; i++) {
            people[i] = i + 1;
        }
        System.out.print("Index: ");
        int pos = 0;
        for (int i = N; i > 1; i--) {
            pos = (pos + (m - 1)) % i;
            System.out.print(people[pos] + " ");
            m = password[pos];
            System.arraycopy(people, pos + 1, people, pos, i - pos - 1);
        }
        System.out.println(people[0]);
    }

    public static void main(String[] args) {
        try {
            Scanner scanner = new Scanner(new File("JosephusInput.txt"));
            int N = scanner.nextInt();
            int m = scanner.nextInt();
            int[] password = new int[N];
            for (int i = 0; i < N; i++) {
                password[i] = scanner.nextInt();
            }
            scanner.close();
            solve(N, m, password);
        } catch (IOException e) {
            System.out.println("读取文件失败: " + e.getMessage());
        }
    }
}
