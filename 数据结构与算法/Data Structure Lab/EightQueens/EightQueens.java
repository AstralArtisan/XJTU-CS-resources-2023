package EightQueens;

import EightQueens.deque.ArrayDeque;
import EightQueens.deque.Deque;

public class EightQueens {
    public static final int SIZE = 8;
    private int[] board = new int[SIZE]; // Save every queen's corresponding column number in every row.
    public Deque<int[]> solutions = new ArrayDeque<>(); // Deque to store all solutions

    /** Make sure there are no queens in the same row, column, or diagonal. */
    private boolean judge(int row) {
        for (int i = 0; i < row; i++) {
            if (board[row] == board[i]) return false; // Same column
            if (Math.abs(row - i) == Math.abs(board[row] - board[i])) return false; // Same diagonal
        }
        return true;
    }

    /** Backtrack to solve the Eight Queens problem. */
    private void backtrack(int row) {
        if (row == SIZE) {
            solutions.addLast(board.clone()); // Save the solution in the deque.
            return;
        }

        for (int col = 0; col < SIZE; col++) {
            board[row] = col; // Place queen
            if (judge(row)) {
                backtrack(row + 1);
            }
        }
    }

    /** Let the user solve the problem. */
    public void solve() {
        backtrack(0);
        System.out.println("Total solutions: " + solutions.size());
    }

    /** Print the current board. */
    public void printSolution(int[] solution) {
        for (int row = 0; row < SIZE; row++) {
            for (int col = 0; col < SIZE; col++) {
                if (solution[row] == col) {
                    System.out.print(" Q "); // Queen
                } else {
                    System.out.print(" · "); // Empty space
                }
            }
            System.out.println();
        }
        System.out.println();
    }

}
