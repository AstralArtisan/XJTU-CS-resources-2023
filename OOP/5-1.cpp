#include <iostream>
#include <cstring>
using namespace std;
/*
1、定义String类，用于存放字符串并实现有关操作，其中有：
数据成员array，类型为char* 。
构造函数String(const char* p)、析构函数~String()。
根据以下main函数及运行结果，实现必要的运算符重载，使得输出符合指定结果。
其中string1[index]表示取字符串中某个字符，因此index的取值范围为0到字符串不算结束符的长度减一，
否则显示“Index out of range.”。而string3+3表示将字符串不算结束符的长度与3相加，且要求不能通过重载+运算符实现。
3、无输入，输出信息为：
mystring
mystrinn
Index out of range.
mystrinnyourstringherstring
mystrinnyourstringherstring
yourstringab
yourstring
12
herstring
*/

class String {
    char* array;
    int length;
public:
    String(const char* p) {
        array = new char[strlen(p) + 1];
        strcpy(array, p);
        length = strlen(array);
    }
    ~String() {
        delete[] array;
    }
    friend String& operator+(String& a, String& b) {
        int len = a.length + b.length;
        char* arr = new char[len + 1];
        strcpy(arr, a.array);
        strcat(arr, b.array);
        delete[] a.array;
        a.array = arr;
        a.length = len;
        return a;
    }
    operator int() {
        return length;
    }
    String operator+(const char* a) {
        int a_len = strlen(a);
        char* new_array = new char[length + a_len + 1];
        strcpy(new_array, array);
        strcat(new_array, a);
        String result(new_array);
        delete[] new_array;
        return result;
    }
    friend ostream& operator<<(ostream& output, const String& a) {
        output << a.array;
        return output;
    }
    char& operator[](int index) {
        if (index < 0 || index >= length) {
            cout << "Index out of range." << endl;
            static char a = '\0';
            return a; // throw "" , main中采用 try{}, catch(const char* ptr) {cout << ptr << endl}
        }
        return array[index];
    }
};

int main(){
    String string1("mystring"),string2("yourstring"),string3("herstring");
    cout<<string1<<endl;
    string1[7]='n';
    cout<<string1<<endl;
    string1[8]='n';
    cout<<string1+string2+string3<<endl;
    cout<<string1<<endl;
    cout<<string2+"ab"<<endl;
    cout<<string2<<endl;
    cout<<string3+3<<endl;
    cout<<string3<<endl;
    return 0;
}