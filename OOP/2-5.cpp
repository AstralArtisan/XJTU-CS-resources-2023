#include <iostream>
#include <cstring>
using namespace std;

/*
学生类：
（1）有姓名（char *name）、分数1（float g1）、分数2（float g2）等数据成员。
（2）有带三个参数（char*,float,float）的构造函数，拷贝构造函数、析构函数。三者执行时要求分别输出字符串"construct"+name、"copy"+name、""destruct"+name。
（3）有返回两个分数、对象地址的三个公有成员函数；不允许有其它成员函数。取对象地址应利用this指针。
main函数：
（1）定义学生对象a，信息是"li" 90.0 80.0。
（2）定义大小为2的学生对象数组st，设置学生信息为：
"zhang" 80.0 70.0
"wang" 90.0 80.0
（3）输出数组中每个学生的平均值：75 85
（4）用a拷贝构造另一个学生对象b，拷贝时将信息修改为："liu" 80.0 70.0。注意"liu"比"li"多了个字符'u'，所以是通过追加字符实现修改，而80.0、70.0又比90.0、80.0都少10.0。
（5）取b地址，并调用b的返回对象地址成员函数，输出两者的差：0
总结：
程序无键盘输入，屏幕输出为（注意次序）：
construct li
construct zhang
construct wang
75 85
destruct wang
destruct zhang
copy liu
0
destruct liu
destruct li
提示：
（1）换行符为endl
（2）对象数组初始化：A a[]={A(...),A(...)}; 或者A *a=new A[2]{A(...),A(...)};
（3）若要在main结束前而不是结束时提前释放对象数组，可以静态定义局部对象数组，或者动态定义对象数组，并主动释放。
*/
class Student {
    char* name;
    float g1, g2;
public:
    Student(const char* name, float g1, float g2);
    Student(const Student& s);
    ~Student();
    float getg1() const {
        return g1;
    }
    float getg2() const {
        return g2;
    }
    Student* getAddr() {
        return this;
    }
};

Student::Student(const char* name, float g1, float g2) {
    this->name = new char[strlen(name) + 1];
    strcpy(this->name, name);
    this->g1 = g1;
    this->g2 = g2;
    cout << "construct" << " "<< name << endl;
}

Student::Student(const Student& s) {
    name = new char[strlen(s.name) + 2];
    strcpy(name, s.name);
    name[strlen(s.name)] = 'u';
    name[strlen(s.name) + 1] = '\0';
    g1 = s.g1 - 10.0f;
    g2 = s.g2 - 10.0f;
    cout << "copy" << " "<< name << endl;
}

Student::~Student() {
    cout << "destruct" << " "<< Student::name << endl;
    delete[] name;
}


int main() {
    Student a = Student("li", 90.0, 80.0);
    Student* st = new Student[2] {Student("zhang", 80.0, 70.0), Student("wang", 90.0, 80.0)};
    cout << (st[0].getg1() + st[0].getg2()) / 2 << " ";
    cout << (st[1].getg1() + st[1].getg2()) / 2;
    cout << endl;
    delete[] st;
    Student b = a;
    cout << &b - b.getAddr() << endl;
    return 0;
}
