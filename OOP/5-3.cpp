#include <iostream>
#include <cstring>
using namespace std;
/*
1、定义A类，有：
私有数据成员a（int）
构造函数（带默认值0）
+运算符重载函数，参数为int，用于将a加上该参数
show函数，显示a的值
2、由A派生B类：
增加私有成员b（int）
构造函数（两个参数，均带默认值0）
+运算符重载函数，参数为int，用于将b加上该参数
show函数，显示a、b的值
3、定义外部函数add，返回为void，带AorB、x（int）两个参数。函数中以运行时多态方式执行语句“AorB+x;”，
  即根据AorB类型，调用A或B的+运算符重载函数。
5、无输入，输出为
a=3
a=2
b=6
*/
class A {
    int a;
public:
    A(int a = 0) {
        this->a = a;
    }
    virtual A& operator+(int b) {
        a += b;
        return *this;
    }
    virtual void show() {
        cout << "a=" << a << endl;
    }
};

class B : public A {
    int b;
public:
    B(int a = 0, int b = 0) : A(a) {
        this->b = b;
    }
    B& operator+(int a) {
        b += a;
        return *this;
    }
    void show() {
        A::show();
        cout << "b=" << b << endl;
    }
};

void add(A& aorb, int x) {
    aorb + x;
}

int main(){
    A myA,*p;
    B myB(2,3);
    add(myA,3); p=&myA;p->show();
    add(myB,3); p=&myB;p->show();
    return 0;
}