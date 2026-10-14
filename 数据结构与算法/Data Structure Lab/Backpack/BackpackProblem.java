package Backpack;
//例如：当T=10，各件物品的体积{1,8,4,3,5,2}时，可找到下列4组解：
//        (1,4,3,2)
//        (1,4,5)
//        (8,2)
//        (3,5,2)。

public class BackpackProblem {
    public int T;  // The capacity of the backpack
    private final int[] items;  // The list of item volumes
    private final Stack current;  // Stack to store the current solution
    private final Stack solutions;  // Stack to simulate storing multiple solutions

    public BackpackProblem(int t, int[] items) {
        this.T = t;
        this.items = items;
        this.current = new Stack();
        this.solutions = new Stack();
    }

    private void save() {
        int[] a = current.toArray();
        solutions.push(current.size());
        for (int j : a) {
            solutions.push(j);
        }
    }

    public void solve() {
        BackTrack(0, 0);
        print();
    }

    private void BackTrack(int i, int sum) {
        if (sum == T) {
            save();
            return;
        }
        if (sum > T || i >= items.length) {
            return;
        }
        current.push(items[i]);
        BackTrack(i + 1, sum + items[i]);
        current.pop();
        BackTrack(i + 1, sum);
    }

    private void print() {
        Stack temp = new Stack();
        while (!solutions.isEmpty()) {
            temp.push(solutions.pop());
        }
        while (!temp.isEmpty()) {
            int i = temp.pop();
            if (i == 0) continue;
            System.out.print("(");
            for (int j = 0; j < i; j++) {
                System.out.print(temp.pop());
                if (j < i - 1) {
                    System.out.print(", ");
                }
            }
            System.out.print(")");
            System.out.println();
        }
    }
}
