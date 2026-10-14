package Backpack;

import java.io.File;
import java.io.IOException;
import java.util.Scanner;

public class Example1 {
    public static void main(String[] args) {
        try {
            Scanner scanner = new Scanner(new File("BPExp1.txt"));
            Stack temp = new Stack();
            int T = scanner.nextInt();
            while (scanner.hasNext()) {
                if (scanner.hasNextInt()) {
                    temp.push(scanner.nextInt());
                } else {
                    scanner.next();
                }
            }
            scanner.close();
            int[] example1 = temp.toArray();
            BackpackProblem prob1 = new BackpackProblem(T, example1);
            prob1.solve();
        } catch (IOException e) {
            System.out.println("读取文件失败: " + e.getMessage());
        }
    }
}
