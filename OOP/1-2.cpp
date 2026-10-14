#include <iostream>
using namespace std;
int main() {
    /* 定义一个函数，参数为三个int类型变量的引用。在函数中交换三个参数的值，使其按升序排序。然后在主函数中输入三个整型数，调用该函数，按升序输出这三个数。 
    例如：
    输入： 3 4 -1
    输出： -1 3 4 
    */
    int a, b, c;
    cin >> a >> b >> c;
    void swap(int& a, int& b, int& c);
    swap(a, b, c);
    cout << a << " " << b << " " << c << endl;
    return 0;
}

void swap(int& a, int& b, int& c) {
    if (a > b) {
        int temp = a;
        a = b;
        b = temp;
    }
    if (a > c) {
        int temp = a;
        a = c;
        c = temp;
    }
    if (b > c) {
        int temp = b;
        b = c;
        c = temp;
    }
}