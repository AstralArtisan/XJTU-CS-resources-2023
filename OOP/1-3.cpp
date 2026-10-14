#include <iostream>
#include <string.h>
using namespace std;
int main() {
    /*
    1、定义常量字符串"abc"，并用指针p指向它，然后输出该常量字符串。
    2、动态创建一个字符数组，并用常变量字符指针q指向它。要求在创建该数组的同时将数组初始化为字符串"def"，即不允许创建后再用赋值语句对其初始化。
       还要求该数组长度能恰好再额外存放一个字符。然后输出该字符串，以及数组长度。
    3、把p指向字符串复制到该数组中，再键盘输入一个字符，追加到字符串尾部，最后输出该字符串。

    实例：
    输入 z
    输出
    abc
    def 5
    abcz

    提示：
    1、常量字符串定义语句char * name ＝"abc"早期C++编译器可正常编译，但后续版本认为有错。
       因为常量字符串”abc”的类型是string literal（字符串文字），而name是指向char变量的指针变量，
       所以直接赋给name有类型不一致问题，而C++11版本不进行隐含类型转换，因此会告警。
       可以改为char * name ＝(char*)"abc"，或者const char * name ＝"abc"
    2、常量字符串不允许修改，否则会报臭名昭著的Segmentation fault错误。
    3、若要修改字符串，则不能定义为常量字符串，可以如下定义：
       char* name=new char[4]{'a','b','c',0};（moodle支持这种初始化，但像vs等不支持）
       使用cout输出name，则输出的是name指向的字符串。
    4、拷贝字符串、计算字符串长度可以使用string.h中的库函数，也可以自己写代码实现。
    5、在提交成功后，可以考虑在程序中增加输出指针变量的值、指针变量本身的地址等内容。注意，
       在输出char*指针变量的值时，需要将其强制转换为void*
    */

    char a;
    cin >> a;

    const char* p = "abc";
    cout << p << endl;

    char* arr = nullptr;
    arr = new char[5]{'d', 'e', 'f', 0};
    int arr_len = strlen(arr);
    const char* q = arr;
    if (arr != nullptr) {
        cout << q << " " << strlen(arr) + 2 << endl;
    }
    else {
        return 1;
    }

    strcpy(arr, p);
    arr[arr_len] = a;
    arr[arr_len + 1] = '\0';
    cout << q << endl;

    if (arr) delete[] arr;
    return 0;
}