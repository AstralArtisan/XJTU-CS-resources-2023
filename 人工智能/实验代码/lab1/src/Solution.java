import java.util.*;

public final class Solution {
    public final boolean solvable;
    public final int moves;
    public final String moveSequence;     // e.g. "URDL..."
    public final List<Board> pathBoards;  // from start -> goal
    public final int expanded;

    public Solution(boolean solvable, int moves, String moveSequence, List<Board> pathBoards, int expanded) {
        this.solvable = solvable;
        this.moves = moves;
        this.moveSequence = moveSequence;
        this.pathBoards = pathBoards;
        this.expanded = expanded;
    }

    public static Solution unsolvable(int expanded) {
        return new Solution(false, -1, "", List.of(), expanded);
    }
}
