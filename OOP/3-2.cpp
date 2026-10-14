#include <iostream>
#include <cstring>
#include <iomanip>
using namespace std;
/*
1、定义一个类，其中有静态数据成员、各种类型非静态数据成员（含字符指针），甚至包括引用（可选），静态和非静态成员函数（含分配空间的构造函数、析构函数）。
2、定义全局对象、main函数中局部对象、另一个被main调用的外部函数func中定义局部对象（可以是形参）、main函数中动态创建对象，每种对象至少2个。观察、分析各种对象地址。
3、输出对象中各个静态与非静态数据成员的值、地址、对象的存储空间大小等信息。由此理解对象的本质、静态数据成员是本类对象共享一份拷贝等问题。此外，应观察对齐现象。
4、（可选）输出对象的每个字节，以揭示引用的实现方法。
5、对于上述各种对象，输出静态、非静态成员函数地址，以及main、func等外部函数的地址，并分析。要求采用合理方法，避免编译器提出警告。
*/
class A {
    static int static_mem; //静态成员
    int normal_mem;        //非静态成员
    char* str_ptr;         //字符指针
    int reference;         //引用指向值
    int& reference_mem;    //引用
public:
    A(int b, const char* c, int d)
     : normal_mem(b), reference(d), reference_mem(reference) {
        //构造函数
        str_ptr = new char[strlen(c) + 1];
        strcpy(str_ptr, c);
        static_mem++; //测试静态成员
    };
    ~A() {
        //析构函数
        delete[] str_ptr;
        static_mem--; //测试静态成员
    }
    static void static_func() {} //静态成员函数
    void normal_func() {}        //普通成员函数
    void print_memory() const {
        cout << setw(8) << "Object at: " << setw(10) << this << " Size: " << sizeof(*this) << endl;                //打印当前对象地址与空间大小
        cout << setw(8) << "  static_mem: " << setw(15) << &static_mem << " (" << static_mem << ")" << endl;       //打印静态成员
        cout << setw(8) << "  normal_mem: " << setw(15) << &normal_mem << " (" << normal_mem << ")" << endl;       //打印非静态成员
        cout << setw(8) << "  str_ptr:    " << setw(15) << (void*)str_ptr << " -> " << str_ptr << endl;           //打印字符指针
        cout << setw(8) << "  ref_mem:    " << setw(15) << &reference_mem << " -> " << reference_mem << endl;      //打印引用成员
        size_t offset_normal = reinterpret_cast<size_t>(&normal_mem) - reinterpret_cast<size_t>(this);
        size_t offset_str = reinterpret_cast<size_t>(&str_ptr) - reinterpret_cast<size_t>(this);
        size_t offset_reference = reinterpret_cast<size_t>(&reference) - reinterpret_cast<size_t>(this);
        size_t offset_ref = reinterpret_cast<size_t>(&reference_mem) - reinterpret_cast<size_t>(this);
        cout << "  Member offsets: " << offset_normal << "/" << offset_str << "/" << offset_reference << "/" << offset_ref << endl;
    }
    static void print_static() {
        cout << setw(8) << "\n  Final static_mem: " << setw(15) << &static_mem << " (" << static_mem << ")" << endl;       //打印静态成员
    }
};

int A::static_mem = 0; //初始化静态成员

//全局对象
A global1(1, "global1", 2); 
A global2(3, "global2", 4);

void func() {
    //外部函数局部对象
    A local1(10, "local1", 100);
    A local2(20, "local2", 200);
    cout << "\n[Function Local Objects]" << endl;
    local1.print_memory();
    local2.print_memory();
}

int main() {
    cout << "======== Memory Analysis ========\n";
    cout << "[Global Objects]" << endl;
    global1.print_memory();
    global2.print_memory();

    // main函数局部对象
    A local1(30, "main_local1", 300);
    A local2(40, "main_local2", 400);

    cout << "\n[Local Objects]" << endl;
    local1.print_memory();
    local2.print_memory();

    // 动态对象
    A* dynamic1 = new A(50, "dynamic1", 500);
    A* dynamic2 = new A(60, "dynamic2", 600);

    cout << "\n[Dynamic Objects]" << endl;
    dynamic1->print_memory();
    dynamic2->print_memory();

    func(); // 传递参数对象
    
    union SafeConverter {
    void (A::*member_ptr)();
    void* raw_ptr;
};

    // 函数地址分析
    cout << "\n======== Function Address ========\n";
    SafeConverter sc;
    sc.member_ptr = &A::normal_func;
    cout << setw(20) << "  Member function:  " << setw(10) << sc.raw_ptr << endl;
    cout << setw(20) << "  Static function:  " << setw(10) << (void*)&A::static_func << endl;
    cout << setw(20) << "  main function:    " << setw(10) << (void*)&main << endl;
    cout << setw(20) << "  func:             " << setw(10) << (void*)&func << endl;
    
    delete dynamic1;
    delete dynamic2;
    
    A::print_static();
    return 0;
}