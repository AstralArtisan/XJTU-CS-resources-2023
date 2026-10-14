#include <windows.h>
#include <iostream>
#include "winio.h"
#include <conio.h>
#include <cstdlib>
#include <ctime>
#include <thread>

#pragma comment(lib,"winio.lib")
using namespace std;
unsigned short BASE_ADDRESS = 0x3F40;
int Port = 16;

//10->2
int create_DI(int(&DI_bit)[8], int num)
{
	int i = 0;
	for (i = 0; i < 8; i++)
		DI_bit[i] = (num >> i) & 0x0001;
	return 0;
}

//2->10
int create_DO(int(&DO_data)[8])
{
	int temp = 0;
	int i = 0;
	for (i = 7; i > 0; i--)
	{
		temp = (temp + DO_data[i]) * 2;
	}
	return temp + DO_data[0];
}

// 设置值
void setVal(char data)
{
	SetPortVal(BASE_ADDRESS + Port, data, 1);
}
void setVal(int(&bits)[8])
{
	setVal(create_DO(bits));
}

//获取输入值
int getNum()
{
	DWORD* p = new DWORD;
	GetPortVal(BASE_ADDRESS + Port, p, 1);
	int num = *p;
	delete p;
	return num;
}
//获取二进制格式的输入值
void getBinaryNum(int(&DI_bit)[8])
{
	create_DI(DI_bit, getNum());
}

void pack()
{
	int base = 1;
	for (int i = 0; i<8; i++)
	{
		setVal(base);
		Sleep(200);
		base <<= 1;
	}
	setVal(0);
}

void main(void)
{
	// 初始化WinIO 
	if (!InitializeWinIo())
	{
		cout << "Error In InitializeWinIo!" << endl;
		exit(1);
	}
	int DI[8] = { 1, 1, 1, 1, 1, 1, 1, 1 };
	int prev_DI0 = 1, prev_DI1 = 1;
	int numOfItems = 0;
	int numOfBoxes = 0;
	while (true)
	{
		if (_kbhit()) {
			break;
		}
		getBinaryNum(DI);
		if (prev_DI0 == 1 && DI[0] == 0)
		{
			numOfItems++;
			setVal(1);
			Sleep(100);
			setVal(0);
			cout << numOfBoxes << "号箱当前已有" << numOfItems << "个物品\n";
		}
		else if (prev_DI1 == 1 && DI[1] == 0)
		{
			if (numOfItems < 8){
				cout << "当前个数不足8个，无法打包\n";
				prev_DI0 = DI[0], prev_DI1 = DI[1];
				continue;
			}
			pack();
			cout << numOfBoxes << "号箱已打包完成\n";
			numOfBoxes++;
			numOfItems = max(0, numOfItems - 8);
			cout << "目前还剩" << numOfItems << "个物品\n";
		}
		prev_DI0 = DI[0], prev_DI1 = DI[1];
	}
}