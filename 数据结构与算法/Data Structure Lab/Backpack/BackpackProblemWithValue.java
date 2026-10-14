package Backpack;

public class BackpackProblemWithValue {
    public int T;  // 背包容量
    public int[] volumes;  // 每件物品的体积
    public int[] values;   // 每件物品的价值
    private final Stack current; // 当前路径
    private int maxValue;  // 最大总价值
    private Stack bestSolution; // 保存最优解的路径

    public BackpackProblemWithValue(int T, int[] volumes, int[] values) {
        this.T = T;
        this.volumes = volumes;
        this.values = values;
        this.current = new Stack();  // 当前尝试的组合
        this.bestSolution = new Stack(); // 最优组合
        this.maxValue = 0;  // 初始化最大价值为 0
    }

    public void solve() {
        BackTrack(0, 0, 0);  // 从第 0 个物品开始，初始容量和价值均为 0
        printBestSolution(); // 打印最优解
    }

    private Stack save() {
        Stack newStack = new Stack();
        int[] a = current.toArray();
        for (int j : a) {
            newStack.push(j);
        }
        return newStack;
    }

    private void BackTrack(int i, int currentVolume, int currentValue) {
        // 递归终止条件
        if (i >= volumes.length) {
            if (currentValue > maxValue) {  // 更新最优解
                maxValue = currentValue;
                bestSolution = save(); // 保存当前最优路径
            }
            return;
        }

        // 选择当前物品（前提：剩余容量足够放下当前物品）
        if (currentVolume + volumes[i] <= T) {
            current.push(i); // 保存物品索引到当前路径
            BackTrack(i + 1, currentVolume + volumes[i], currentValue + values[i]);
            current.pop(); // 回溯，撤销选择
        }

        // 不选择当前物品
        BackTrack(i + 1, currentVolume, currentValue);
    }

    private void printBestSolution() {
        System.out.println("Maximum Value: " + maxValue);
        System.out.print("Items in the best solution: ");
        while (!bestSolution.isEmpty()) {
            int item = bestSolution.pop();
            System.out.print("Item " + item + " (Volume: " + volumes[item] + ", Value: " + values[item] + ") ");
        }
        System.out.println();
    }
}