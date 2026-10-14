package BST_AVL;

import org.junit.Before;
import org.junit.Test;
import static org.junit.Assert.*;

public class ArrayBSTTest {

    private ArrayBST bst;

    // 在每个测试方法执行之前初始化
    @Before
    public void setUp() {
        // 每个测试之前都创建一个新的空树
        bst = new ArrayBST();
    }

    @Test
    public void testDeleteLeafNode() {
        // 测试删除一个叶子节点
        bst.insert(10);
        bst.insert(5);
        bst.insert(15);

        bst.delete(5);  // 删除叶子节点

        // 确保树的结构正确，5已经删除
        assertNull(bst.arr.get(1));  // 5 应该被删除
        assertEquals(Integer.valueOf(10), bst.arr.get(0));  // 根节点应该还是 10
        assertEquals(Integer.valueOf(15), bst.arr.get(2));  // 15 还在树上
    }

    @Test
    public void testDeleteNodeWithOneChild() {
        // 测试删除一个有一个子节点的节点
        bst.insert(10);
        bst.insert(5);
        bst.insert(15);
        bst.insert(12);

        bst.delete(15);  // 删除有一个左子节点的节点 15

        // 确保 15 被删除，并且 12 成为新的 15
        assertNull(bst.arr.get(5));  // 12 应该被删除
        assertEquals(Integer.valueOf(12), bst.arr.get(2));  // 12 现在应该替代 15
    }

    @Test
    public void testDeleteNodeWithTwoChildren() {
        // 测试删除一个有两个子节点的节点
        bst.insert(10);
        bst.insert(5);
        bst.insert(15);
        bst.insert(3);
        bst.insert(7);
        bst.insert(12);
        bst.insert(18);

        bst.delete(5);  // 删除有两个子节点的节点 5

        // 确保删除后的树结构正确
        assertNull(bst.arr.get(1));  // 5 应该被删除
        assertEquals(Integer.valueOf(7), bst.arr.get(1));  // 7 成为新的节点
        assertEquals(Integer.valueOf(3), bst.arr.get(3));  // 3 依然存在
    }

    @Test
    public void testDeleteNonExistentNode() {
        // 测试删除一个不存在的节点
        bst.insert(10);
        bst.insert(5);
        bst.insert(15);

        bst.delete(20);  // 20 不在树中

        // 确保树结构没有变化
        assertEquals(Integer.valueOf(10), bst.arr.get(0));  // 根节点仍然是 10
        assertEquals(Integer.valueOf(5), bst.arr.get(1));  // 左子节点 5 仍然存在
        assertEquals(Integer.valueOf(15), bst.arr.get(2));  // 右子节点 15 仍然存在
    }
}
