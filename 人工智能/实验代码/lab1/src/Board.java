import java.util.*;

public final class Board {
    private final int n;          // dimension (for 8-puzzle it's 3)
    private final int[] tiles;    // length n*n, 0 is blank
    private final int blank;      // index of 0
    private final int manhattan;  // cached heuristic

    private static final int N_GOAL = 3;
    private static final int[] GOAL = {
            1, 2, 3,
            8, 0, 4,
            7, 6, 5
    };

    // goal position lookup: value -> index
    private static final int[] GOAL_POS = buildGoalPos();

    private static int[] buildGoalPos() {
        int[] pos = new int[N_GOAL * N_GOAL];
        Arrays.fill(pos, -1);
        for (int i = 0; i < GOAL.length; i++) pos[GOAL[i]] = i;
        return pos;
    }

    public Board(int n, int[] tiles) {
        if (n <= 0) throw new IllegalArgumentException("n must be > 0");
        if (tiles == null || tiles.length != n * n) throw new IllegalArgumentException("tiles length must be n*n");
        if (n != N_GOAL) throw new IllegalArgumentException("This solver is configured for 3x3 goal only.");

        this.n = n;
        this.tiles = Arrays.copyOf(tiles, tiles.length);

        int b = -1;
        boolean[] seen = new boolean[n * n];
        for (int i = 0; i < this.tiles.length; i++) {
            int v = this.tiles[i];
            if (v < 0 || v >= n * n) throw new IllegalArgumentException("tile value out of range: " + v);
            if (seen[v]) throw new IllegalArgumentException("duplicate tile: " + v);
            seen[v] = true;
            if (v == 0) b = i;
        }
        if (b == -1) throw new IllegalArgumentException("no blank(0) tile");
        this.blank = b;

        this.manhattan = computeManhattan();
    }

    public int n() { return n; }
    public int[] tilesCopy() { return Arrays.copyOf(tiles, tiles.length); }
    public int blankIndex() { return blank; }
    public int manhattan() { return manhattan; }

    public boolean isGoal() {
        return Arrays.equals(this.tiles, GOAL);
    }

    public boolean isSolvable() {
        // General rule (works for arbitrary goal):
        // - odd width: inv(start) parity must equal inv(goal) parity
        // - even width: (inv + blankRowFromBottom) parity must match goal's same value
        int invStart = inversionCount(this.tiles);
        int invGoal  = inversionCount(GOAL);

        if (n % 2 == 1) {
            return (invStart % 2) == (invGoal % 2);
        } else {
            int startBlankRowFromBottom = n - (blank / n);
            int goalBlankIndex = GOAL_POS[0];
            int goalBlankRowFromBottom = n - (goalBlankIndex / n);

            int startKey = (invStart + startBlankRowFromBottom) % 2;
            int goalKey  = (invGoal  + goalBlankRowFromBottom) % 2;
            return startKey == goalKey;
        }
    }

    private int inversionCount(int[] arr) {
        int inv = 0;
        for (int i = 0; i < arr.length; i++) {
            if (arr[i] == 0) continue;
            for (int j = i + 1; j < arr.length; j++) {
                if (arr[j] == 0) continue;
                if (arr[i] > arr[j]) inv++;
            }
        }
        return inv;
    }

    private int computeManhattan() {
        int dist = 0;
        for (int idx = 0; idx < tiles.length; idx++) {
            int v = tiles[idx];
            if (v == 0) continue;

            int goalIdx = GOAL_POS[v];
            int goalR = goalIdx / n;
            int goalC = goalIdx % n;

            int r = idx / n;
            int c = idx % n;

            dist += Math.abs(r - goalR) + Math.abs(c - goalC);
        }
        return dist;
    }

    public List<Neighbor> neighbors() {
        int r = blank / n;
        int c = blank % n;
        List<Neighbor> res = new ArrayList<>(4);
        for (Move m : Move.values()) {
            int nr = r + m.dr;
            int nc = c + m.dc;
            if (nr < 0 || nr >= n || nc < 0 || nc >= n) continue;
            int nb = nr * n + nc;

            int[] next = Arrays.copyOf(tiles, tiles.length);
            next[blank] = next[nb];
            next[nb] = 0;

            res.add(new Neighbor(m, new Board(n, next)));
        }
        return res;
    }

    public static final class Neighbor {
        public final Move move;
        public final Board board;
        public Neighbor(Move move, Board board) {
            this.move = move;
            this.board = board;
        }
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (!(o instanceof Board other)) return false;
        return this.n == other.n && Arrays.equals(this.tiles, other.tiles);
    }

    @Override
    public int hashCode() {
        int result = Integer.hashCode(n);
        result = 31 * result + Arrays.hashCode(tiles);
        return result;
    }

    @Override
    public String toString() {
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < tiles.length; i++) {
            if (i % n == 0) sb.append('\n');
            sb.append(String.format("%2d ", tiles[i]));
        }
        return sb.toString();
    }
}
