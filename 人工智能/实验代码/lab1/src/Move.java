public enum Move {
    U(-1, 0, 'U'),
    D(1, 0, 'D'),
    L(0, -1, 'L'),
    R(0, 1, 'R');

    public final int dr;
    public final int dc;
    public final char code;

    Move(int dr, int dc, char code) {
        this.dr = dr;
        this.dc = dc;
        this.code = code;
    }
}
