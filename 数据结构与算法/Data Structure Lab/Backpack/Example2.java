package Backpack;

import java.io.File;
import java.io.IOException;
import java.util.Scanner;

//20
//5 9 6 2 3 7 10 12 1 8 11 18
public class Example2 {
    public static void main(String[] args) {
        try {
            Scanner scanner = new Scanner(new File("BPExp2.txt"));
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
            int[] example2 = temp.toArray();
            BackpackProblem prob1 = new BackpackProblem(T, example2);
            prob1.solve();
        } catch (IOException e) {
            System.out.println("读取文件失败: " + e.getMessage());
        }
    }
}
