#include <iostream>
using namespace std;

// 封装单个数组的类
class Array {
private:
    int* data;  // 动态数组
    int size;   // 数组大小

public:
    // 构造函数
    Array(int* arr, int size) : data(arr), size(size) {}

    // 重载 [] 运算符
    int& operator[](int index) {
        if (index < 0 || index >= size) {
            throw out_of_range("Index out of bounds");
        }
        return data[index];
    }
};

// 主类，包含两个数组
class MyArray {
private:
    int arr_data[5] = {0, 1, 2, 3, 4};  // 数组 1
    int arr2_data[5] = {5, 6, 7, 8, 9}; // 数组 2

public:
    Array arr;   // 暴露封装的数组 1
    Array arr2;  // 暴露封装的数组 2

    // 构造函数，初始化两个数组
    MyArray() : arr(arr_data, 5), arr2(arr2_data, 5) {}
};

int main() {
    MyArray myarr;

    // 访问 arr 数组
    cout << myarr.arr[0] << endl;  // 输出 0
    cout << myarr.arr[4] << endl;  // 输出 4

    // 访问 arr2 数组
    cout << myarr.arr2[0] << endl; // 输出 5
    cout << myarr.arr2[4] << endl; // 输出 9

    

    return 0;
}
