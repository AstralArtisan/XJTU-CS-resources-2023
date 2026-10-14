#include <windows.h>
#include <iostream>

#include "winio.h"
#include <conio.h>
#include <cstdlib>

#pragma comment(lib,"winio.lib")
using namespace std;

//10->2
int creat_DI(int(&DI_bit)[8], int num)
{
	int i = 0;
	for (i = 0; i<8; i++)
		DI_bit[i] = (num >> i) & 0x0001;
	return 0;
}

//2->10
int creat_DO(int(&DO_data)[8])
{
	int temp = 0;
	int i = 0;
	for (i = 7; i>0; i--)
	{
		temp = (temp + DO_data[i]) * 2;
	}
	return temp + DO_data[0];
}

int creat_BCD(int num) {
	int num0 = num % 10;
	int num1 = num / 10;
	return (num1 << 4) | num0;
}

void main(void)
{
	char c;
	unsigned short BASE_ADDRESS = 0x3F40;
	int Port = 16;

	// 初始化WinIO 
	if (!InitializeWinIo())
	{
		cout << "Error In InitializeWinIo!" << endl;
		exit(1);
	}
	int i;
	char DI_data;
	DWORD *p = new DWORD;
	int DI[8] = { 1 };
	int DO[8] = { 0 };
	int car_max = 50;
	int car = 50;
	int curr_DI0 = DI[0];
	int curr_DI1 = DI[1];
	while (1) {
		if (_kbhit()){
			break;
		}
		GetPortVal(BASE_ADDRESS + Port, p, 1);
		DI_data = *p;
		creat_DI(DI, DI_data);
		
		if (!DI[0] && curr_DI0) {
			if (car == 0) 
				cout << "停车场已满，无法进车！" << endl;
			else car--;
		}
		if (!DI[1] && curr_DI1) {
			if (car == 50) 
				cout << "停车场空闲，无法出车！" << endl;
			else car++;
		}
		
		curr_DI0 = DI[0];
		curr_DI1 = DI[1];
		SetPortVal(BASE_ADDRESS + Port, creat_BCD(car), 1);
	}
	ShutdownWinIo();
}