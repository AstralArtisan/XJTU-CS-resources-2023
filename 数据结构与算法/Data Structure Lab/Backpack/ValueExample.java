package Backpack;

public class ValueExample {
    public static void main(String[] args) {
        int T = 10; // 背包容量
        int[] volumes = {1, 8, 4, 3, 5, 2}; // 每件物品的体积
        int[] values = {10, 40, 30, 50, 35, 25}; // 每件物品的价值

        BackpackProblemWithValue prob = new BackpackProblemWithValue(T, volumes, values);
        prob.solve();
    }
}
