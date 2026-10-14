
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
	char DO_data;
	DWORD *p = new DWORD;


	int DI[8] = { 0 };
	int DO[8] = { 0 };
	GetPortVal(BASE_ADDRESS + Port, p, 1);
	DI_data = *p;
	DO_data = 255;
	SetPortVal(BASE_ADDRESS + Port, DO_data, 1);
	bool status = true;
	cout << "默认对应熄灭，按s切换状态，其他键退出\n";
	while (1)
	{
		
		if (_kbhit()){
			c = _getch();
			if (c == 'S' || c == 's')
				status = !status;
			else break;
		}
		
		if (status){
			GetPortVal(BASE_ADDRESS + Port, p, 1);
			DI_data = *p;
			creat_DI(DI, DI_data);
			for (int i = 0; i <= 7; i++){
				DO[i] = DI[7-i];
			}
			DO_data = creat_DO(DO);
			SetPortVal(BASE_ADDRESS + Port, DO_data, 1);
		}
		else{
			GetPortVal(BASE_ADDRESS + Port, p, 1);
			DI_data = *p;
			creat_DI(DI, DI_data);
			for (int i = 0; i <= 7; i++){
				DO[i] = !DI[7-i];
			}
			DO_data = creat_DO(DO);
			SetPortVal(BASE_ADDRESS + Port, DO_data, 1);
		}
	}
	SetPortVal(BASE_ADDRESS + Port, 0, 1);
	ShutdownWinIo();		//关闭WinIO	

}

