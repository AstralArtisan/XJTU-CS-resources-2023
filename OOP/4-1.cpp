#include <iostream>
using namespace std;
/*
    1、定义基类Base，有私有数据成员x，int类型。有带一个参数的构造函数为其初始化。有show函数，显示x的值。
    2、定义公有派生类derived，增加私有数据成员y，int类型，有带两个参数的构造函数，为其数据成员初始化。有show函数，显示x、y的值。
    3、定义外部函数func1、func2，形参分别是Base、derived类对象，函数只有一句，调用形参对象的show函数。
    4、在main函数中静态定义derived类对象a，实参为10、20，并调用其show函数。然后分别定义Base指针、derived指针，都指向a，分别用指针调用其show函数。
    最后以a为实参，调用函数func1、func2。
    5、程序无输入，输出为：
    x=10
    y=20
    x=10
    x=10
    y=20
    x=10
    x=10
    y=20
*/
class Base {
    int x;
public:
    Base(int x) {
        this -> x = x;
    }
    void show() {
        cout << "x=" << x << endl;
    }
};

class derived : public Base {
    int y;
public:
    derived(int x, int y) : Base(x) {
        this -> y = y;
    }
    void show() {
        Base::show();
        cout << "y=" << y << endl;
    }
};

void func1(Base a) {
    a.show();
}

void func2(derived b) {
    b.show();
}

int main() {
    static derived a(10, 20);
    a.show();
    Base* p1 = &a;
    derived* p2 = &a;
    p1 -> show();
    p2 -> show();
    func1(a);
    func2(a);
    return 0;
}