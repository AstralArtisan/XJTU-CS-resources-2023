import java.util.*;

public class Main {

    public static void main(String[] args) {
        // Input: 9 integers (0..8), 0 is blank. Can be in one line or multiple lines.
        // Example:
        // 1 2 3
        // 4 5 6
        // 7 0 8
        Scanner sc = new Scanner(System.in);

        // ----- choose solver -----
        System.out.println("Choose search method:");
        System.out.println("  1) A* Search");
        System.out.println("  2) BFS (Breadth-First Search)");
        System.out.print("Enter 1 or 2 (default 1): ");

        String choiceLine = sc.nextLine().trim();
        int choice = 1;
        if (!choiceLine.isEmpty()) {
            try {
                choice = Integer.parseInt(choiceLine);
            } catch (NumberFormatException ignored) {
                choice = 1;
            }
        }
        Solver solver;
        if (choice == 2) {
            solver = new BFSsolver();
            System.out.println(">> Using BFS");
        } else {
            solver = new AStarSolver();
            System.out.println(">> Using A*");
        }

        // ----- read board -----
        System.out.println("\nPlease input 9 integers (0..8), where 0 is the blank.");
        System.out.println("You can input in one line or multiple lines. Example:");
        System.out.println("1 2 3");
        System.out.println("4 5 6");
        System.out.println("7 0 8");
        System.out.print("Input: ");

        int n = 3;
        int[] tiles = new int[n * n];
        int count = 0;

        while (count < tiles.length && sc.hasNextInt()) {
            tiles[count++] = sc.nextInt();
        }

        if (count < tiles.length) {
            System.out.println("Input error: please provide 9 integers (0..8), where 0 is the blank.");
            System.out.println("Example:\n1 2 3\n4 5 6\n7 0 8");
            return;
        }

        Board start;
        try {
            start = new Board(n, tiles);
        } catch (IllegalArgumentException e) {
            System.out.println("Invalid board: " + e.getMessage());
            return;
        }

        System.out.println("Start:" + start);
        if (choice != 2)  System.out.println("Heuristic (Manhattan) = " + start.manhattan());

        // ----- solve -----
        Solution sol = solver.solve(start);

        if (!sol.solvable) {
            System.out.println("\nThis puzzle is UNSOLVABLE.");
            System.out.println("Expanded nodes = " + sol.expanded);
            return;
        }

        System.out.println("\nSOLVED!");
        System.out.println("Moves = " + sol.moves);
        System.out.println("Move sequence = " + sol.moveSequence);
        System.out.println("Expanded nodes = " + sol.expanded);

        System.out.println("\nPath:");
        for (int i = 0; i < sol.pathBoards.size(); i++) {
            System.out.println("Step " + i + ":" + sol.pathBoards.get(i));
        }
    }
}
