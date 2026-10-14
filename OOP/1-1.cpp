#include <iostream>
using namespace std;
int main() {
    /*先创建一个数组，数组长度由键盘输入确定，数组类型为指向int的指针。
      然后动态创建一组int类型变量，使得数组每个元素指向一个变量，并通过键盘输入每个变量的值。
      最后输出这些变量的和。
    例如：
    输入 3 7 3 -1 //第1个数为数组大小
    输出 9 
    提示：创建数组时需要先定义指向指针的指针变量；
    注意：应遵守防止使用野指针的有关措施。
    */

    int x; // Length
    cin >> x;    
    int** arr = new int*[x]; // Pointer to int pointer
    
    for (int i = 0; i < x; i++) {
        // Create a new int variable and assign it to the pointer
        arr[i] = new int;
        cin >> *arr[i];
    }

    int sum = 0;
    for (int i = 0; i < x; i++) {
        sum += *arr[i];
    }

    cout << sum << endl;
    delete[] arr;
    return 0;
}