import java.util.*;

public final class BFSsolver extends Solver {

    @Override
    public Solution solve(Board start) {
        if (!start.isSolvable()) {
            return Solution.unsolvable(0);
        }
        if (start.isGoal()) {
            return new Solution(true, 0, "", List.of(start), 0);
        }

        // BFS queue (FIFO)
        ArrayDeque<Node> q = new ArrayDeque<>();

        // visited set: first time we see a Board is the shortest path (unit cost)
        HashSet<Board> visited = new HashSet<>();

        Node startNode = new Node(start, 0, null, null);
        q.add(startNode);
        visited.add(start);

        int expanded = 0;

        while (!q.isEmpty()) {
            Node cur = q.poll();
            expanded++;

            if (cur.board.isGoal()) {
                return buildSolution(cur, expanded);
            }

            for (Board.Neighbor nb : cur.board.neighbors()) {
                if (visited.add(nb.board)) { // true if first time seen
                    q.add(new Node(nb.board, cur.g + 1, cur, nb.move));
                }
            }
        }

        // If solvable, BFS should eventually find the goal.
        return Solution.unsolvable(expanded);
    }
}
