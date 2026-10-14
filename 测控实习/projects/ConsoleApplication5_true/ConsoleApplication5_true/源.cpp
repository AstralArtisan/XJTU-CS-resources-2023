#include <windows.h>
#include <iostream>
#include "winio.h"
#include <conio.h>
#include <cstdlib>
#include <random>
#include <ctime>

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

void main(void)
{
	// 初始化WinIO 
	if (!InitializeWinIo())
	{
		cout << "Error In InitializeWinIo!" << endl;
		exit(1);
	}
	int DI[8] = { 1,1,1,1,1,1,1,1 };
	int prev_DI0 = DI[0], prev_DI1 = DI[1];
	bool isWork = false;
	bool endwork = false;
	bool startwork = true;
	int DO[8] = { 0 };
	while (true)
	{
		if (_kbhit()) {
			break;
		}
		getBinaryNum(DI);
		if (prev_DI0 == 1 && DI[0] == 0){
			startwork = true;
			isWork = true;
		}
		if (prev_DI1 == 1 && DI[1] == 0){
			isWork = false;
			endwork = true;
		}
			
		if (!DI[7])
		{
			setVal(0);
			return;
		}
		prev_DI0 = DI[0];
		prev_DI1 = DI[1];
		if (isWork)
		{
			if (startwork){
				Sleep(1000);
				DO[0] = 1;
				setVal(DO);
				Sleep(1000);
				DO[1] = 1;
				setVal(DO);
				Sleep(1000);
				DO[2] = 1;
				setVal(DO);
				startwork = false;
			}
			getBinaryNum(DI);
			prev_DI0 = DI[0];
			prev_DI1 = DI[1];
			int numOfLoads = (!DI[5]) + (!DI[4] << 1) + (!DI[3] << 2) + (!DI[2] << 3);
			if (numOfLoads <= 10 && numOfLoads >= 0)
			{
				DO[3] = 1;
				DO[4] = 0;
				DO[5] = 0;
				setVal(DO);
			}
			else if (numOfLoads <= 13)
			{
				DO[3] = 0;
				DO[4] = 1;
				DO[5] = 0;
				setVal(DO);
			}
			else
			{
				DO[3] = 0;
				DO[4] = 0;
				DO[5] = 1;
				setVal(DO);
				auto start = time(nullptr);
				bool solved = false;
				while (difftime(time(nullptr), start) <= 3)
				{
					getBinaryNum(DI);
					prev_DI0 = DI[0];
					prev_DI1 = DI[1];
					numOfLoads = (!DI[5]) + (!DI[4] << 1) + (!DI[3] << 2) + (!DI[2] << 3);
					if (numOfLoads <= 13){
						solved = true;
						break;
					}
				}
				if (solved)
					continue;
				isWork = false;
				endwork = true;
			}
		}
		else if (endwork)
		{
			for (int i = 3; i < 7; i++)
				DO[i] = 0;
			Sleep(1000);
			DO[2] = 0;
			setVal(DO);
			Sleep(1000);
			DO[1] = 0;
			setVal(DO);
			Sleep(1000);
			DO[0] = 0;
			setVal(DO);
			endwork = false;
		}
	}
}