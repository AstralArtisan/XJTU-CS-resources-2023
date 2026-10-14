package EightQueens;

import java.util.Random;

public class RandomSolution extends EightQueens {
    private void printRandomSolution() {
        if (solutions.isEmpty()) {
            System.out.println("No solutions available.");
            return;
        }

        Random random = new Random();
        int randomIndex = random.nextInt(solutions.size());
        System.out.println("One solution:");
        printSolution(solutions.get(randomIndex));
    }

    public static void main(String[] args) {
        RandomSolution eq = new RandomSolution();
        eq.solve();
        eq.printRandomSolution();
    }
}
