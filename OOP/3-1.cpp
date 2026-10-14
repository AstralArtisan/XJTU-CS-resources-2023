#include <iostream>
using namespace std;
/*
    定义学生类：
    （1）有平时（int）、期末成绩（int）、组成比例（float）三个私有数据成员；
    （2）有成员函数setProp，根据键盘输入，设置组成比例，取值在0到1之间，比如0.4，则表示成绩=平时*40%+期末*60%；
    （3）有计算并输出成绩函数compScore，根据组成比例计算并输出成绩。
    定义教师类：
    （1）有私有数据成员pStu，是学生对象指针；
    （2）有构造函数，带整型参数，以其值为大小创建学生对象数组，用pStu指向；
    （3）有成员函数assign，参数为数组下标，接收键盘输入，设置一个数组元素（学生对象）的平时、期末成绩。
    （4）有成员函数show，参数为数组下标，用于调用一个学生对象的compScore函数，显示其成绩。
    main函数中：
    （1）先调用学生类的setProp函数，键盘输入并设置组成比例；
    （2）键盘输入学生数，由此定义教师对象。
    （3）调用assign函数，为数组中每个学生对象给平时、期末成绩，两个成绩由键盘输入；
    （4）调用show函数，逐个计算并输出每个学生的成绩，以空格隔开。
    举例1：
    输入：0.4 3 70 90 60 80 55 88
    输出：82 72 74.8
    举例2：
    输入：0.5 4 70 90 60 80 55 88 66 77
    输出：80 70 71.5 71.5
    要求：1、本题禁止使用友元函数、友元类。2、也不允许有其它成员函数，比如setter/getter函数。
*/

class student {
private:
    int performScore;
    int finalScore;
    static float scale; //此处使用静态成员，因为所有学生的成绩组成比例都是一样的
public:
    student(int p, int f) {
        performScore = p;
        finalScore = f;
    }
    student() {};
    static void setProp() {
        //此处使用静态函数，与静态成员保持一致
        cin >> scale;
    }
    void compScore() {
        float score = performScore * scale + finalScore * (1 - scale);
        cout << score << " ";
    }
};
float student::scale = 0.0; // 注意静态成员必须初始化！

class teacher {
private:
    student* pStu;
public:
    teacher(int n) {
        pStu = new student[n];
    }
    ~teacher() {
        delete[] pStu;
    }
    void assign(int i) {
        int pScore, fScore;
        cin >> pScore >> fScore;
        pStu[i] = student(pScore, fScore);
    }
    void show(int i) {
        pStu[i].compScore();
    }
};

int main() {
    student::setProp();
    int n;
    cin >> n;
    teacher t(n);
    for (int i = 0; i < n; i++) {
        t.assign(i);
    }
    for (int i = 0; i < n; i++) {
        t.show(i);
    }
    return 0;
}