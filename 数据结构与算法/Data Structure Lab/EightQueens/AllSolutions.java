package EightQueens;

public class AllSolutions extends EightQueens {
    private void printAllSolutions() {
        if (solutions.isEmpty()) {
            System.out.println("No solutions available.");
            return;
        }

        for (int i = 0; i < solutions.size(); i++) {
            System.out.println("Solution " + (i + 1) + ":");
            printSolution(solutions.get(i));
        }
    }
    public static void main(String[] args) {
        AllSolutions eq = new AllSolutions();
        eq.solve();
        eq.printAllSolutions();
    }
}
