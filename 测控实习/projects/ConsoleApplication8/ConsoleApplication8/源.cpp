#include <windows.h>
#include <iostream>
#include "winio.h"
#include <conio.h>
#include <cstdlib>
#include <ctime>

#pragma comment(lib,"winio.lib")

#define NEG_EDGE(pos) (rec[pos]==1 && DI[pos]==0)

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
void setVal(int data)
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

int rec[8] = { 1, 1, 1, 1, 1, 1, 1, 1 };
int DI[8] = { 1, 1, 1, 1, 1, 1, 1, 1 };

//保存当前值
void saveVal()
{
	for (int i = 0; i<8; i++)
	{
		rec[i] = DI[i];
	}
}

void down()
{
	setVal((1 << 5) + (1 << 3));
	cout << "下行\n";
}

void up()
{
	setVal((1 << 4) + (1 << 3));
	cout << "上行\n";
}

void at1AndOpen()
{
	setVal(1 + (1 << 2));
	cout << "一楼开\n";
}

void at2AndOpen()
{
	setVal(2 + (1 << 2));
	cout << "二楼开\n";
}

void at1AndClose()
{
	setVal(1 + (1 << 3));
	cout << "一楼关\n";
}

void at2AndClose()
{
	setVal(2 + (1 << 3));
	cout << "二楼关\n";
}

void main(void)
{
	// 初始化WinIO 
	if (!InitializeWinIo())
	{
		cout << "Error In InitializeWinIo!" << endl;
		exit(1);
	}
	//记录状态
	int call = 0;
	int ele = 0;
	while (ele == 0)
	{
		saveVal();
		getBinaryNum(DI);
		if (DI[2] == 0)
		{
			at1AndClose();
			ele = 1;
		}
		else if (DI[3] == 0)
		{
			at2AndClose();
			ele = 2;
		}
	}
	while (true)
	{
		if (_kbhit()) {
			break;
		}
		saveVal();
		getBinaryNum(DI);
		if (NEG_EDGE(0) && call == 0)
			call = 1;
		else if (NEG_EDGE(1) && call == 0)
			call = 2;
		if (call == 1)
		{
			if (ele == 2)
			{
				saveVal();
				getBinaryNum(DI);
				while (DI[3] == 0)
				{
					saveVal();
					getBinaryNum(DI);
				}
				//下行
				down();

			}
			//到达一层，开门
			saveVal();
			getBinaryNum(DI);
			while (DI[2] == 1)
			{
				saveVal();
				getBinaryNum(DI);
			}
			ele = 1;
			at1AndOpen();


			//关门
			auto start = time(nullptr);
			bool closed = false;
			while (difftime(time(nullptr), start) <= 5)
			{
				saveVal();
				getBinaryNum(DI);
				if (DI[4] == 0)
				{
					closed = true;
					break;
				}
			}
			at1AndClose();
			//停止
			if (!closed)
			{
				call = 0;
				continue;
			}

			saveVal();
			getBinaryNum(DI);
			while (DI[2] == 0)
			{
				saveVal();
				getBinaryNum(DI);
			}
			//上行
			up();
			//到达2层开门
			saveVal();
			getBinaryNum(DI);
			while (DI[3] == 1)
			{
				saveVal();
				getBinaryNum(DI);
			}
			ele = 2;
			at2AndOpen();
			//关门
			Sleep(5000);
			at2AndClose();
			call = 0;
		}
		else if (call == 2)
		{
			if (ele == 1)
			{
				saveVal();
				getBinaryNum(DI);
				while (DI[2] == 0)
				{
					saveVal();
					getBinaryNum(DI);
				}
				//上行
				up();

			}
			//到达2层，开门
			saveVal();
			getBinaryNum(DI);
			while (DI[3] == 1)
			{
				saveVal();
				getBinaryNum(DI);
			}
			ele = 2;
			at2AndOpen();


			//关门
			auto start = time(nullptr);
			bool closed = false;
			while (difftime(time(nullptr), start) <= 5)
			{
				saveVal();
				getBinaryNum(DI);
				if (DI[4] == 0)
				{
					closed = true;
					break;
				}
			}
			at2AndClose();
			//停止
			if (!closed)
			{
				call = 0;
				continue;
			}

			saveVal();
			getBinaryNum(DI);
			while (DI[3] == 0)
			{
				saveVal();
				getBinaryNum(DI);
			}
			//下行
			down();
			//到达1层开门
			saveVal();
			getBinaryNum(DI);
			while (DI[2] == 1)
			{
				saveVal();
				getBinaryNum(DI);
			}
			ele = 1;
			at1AndOpen();
			//关门
			Sleep(5000);
			at1AndClose();
			call = 0;
		}
	}
}