import java.util.*;

public final class AStarSolver extends Solver {

    @Override
    public Solution solve(Board start) {
        if (!start.isSolvable()) {
            return Solution.unsolvable(0);
        }
        if (start.isGoal()) {
            return new Solution(true, 0, "", List.of(start), 0);
        }

        PriorityQueue<Node> open = new PriorityQueue<>((a, b) -> {
            if (a.f != b.f) return Integer.compare(a.f, b.f);
            if (a.h != b.h) return Integer.compare(a.h, b.h);
            return Integer.compare(b.g, a.g); // tie-break: prefer deeper if same f/h
        });

        // best known g for each board
        HashMap<Board, Integer> bestG = new HashMap<>();
        Node startNode = new Node(start, 0, null, null);
        open.add(startNode);
        bestG.put(start, 0);

        int expanded = 0;

        while (!open.isEmpty()) {
            Node cur = open.poll();

            Integer bg = bestG.get(cur.board);
            if (bg == null || cur.g != bg) {
                continue; // outdated entry
            }

            expanded++;

            if (cur.board.isGoal()) {
                return buildSolution(cur, expanded);
            }

            for (Board.Neighbor nb : cur.board.neighbors()) {
                int ng = cur.g + 1;
                Integer old = bestG.get(nb.board);
                if (old == null || ng < old) {
                    bestG.put(nb.board, ng);
                    open.add(new Node(nb.board, ng, cur, nb.move));
                }
            }
        }

        // For solvable puzzles, A* with consistent heuristic should find a goal.
        return Solution.unsolvable(expanded);
    }
}
