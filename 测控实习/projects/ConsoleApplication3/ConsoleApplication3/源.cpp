#include <windows.h>
#include <iostream>
#include "winio.h"
#include <conio.h>
#include <cstdlib>
#include <random>

#pragma comment(lib,"winio.lib")
using namespace std;
unsigned short BASE_ADDRESS = 0x3F40;
int Port = 16;

//10->2
int create_DI(int(&DI_bit)[8], int num)
{
	int i = 0;
	for (i = 0; i<8; i++)
		DI_bit[i] = (num >> i) & 0x0001;
	return 0;
}

//2->10
int create_DO(int(&DO_data)[8])
{
	int temp = 0;
	int i = 0;
	for (i = 7; i>0; i--)
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
	random_device rd;
	mt19937 gen(rd());
	uniform_int_distribution<> num_dis(0, 7);
	uniform_int_distribution<> time_dis(300, 1000);
	int DI[8] = { 1 };
	bool gate = false;
	int prevDI0 = 1;
	while (1) {
		if (_kbhit()){
			break;
		}
		getBinaryNum(DI);
		if (DI[0]==0 && prevDI0==1) gate = !gate;
		prevDI0 = DI[0];
		if (gate) {
			getBinaryNum(DI);
			prevDI0 = DI[0];
			if (!DI[1]) {
				int DO[8] = { 0 };
				setVal(DO);
				while (true) {
					getBinaryNum(DI);
					prevDI0 = DI[0];
					if (DI[1]) break;
					DO[0] = 1;
					setVal(DO);
					Sleep(500);
					DO[7] = 1;
					setVal(DO);
					Sleep(500);
					getBinaryNum(DI);
					prevDI0 = DI[0];
					if (DI[1]) break;
					DO[1] = 1;
					setVal(DO);
					Sleep(500);
					DO[6] = 1;
					setVal(DO);
					Sleep(500);
					getBinaryNum(DI);
					prevDI0 = DI[0];
					if (DI[1]) break;
					DO[2] = 1;
					setVal(DO);
					Sleep(500);
					DO[5] = 1;
					setVal(DO);
					Sleep(500);
					getBinaryNum(DI);
					prevDI0 = DI[0];
					if (DI[1]) break;
					DO[3] = 1;
					setVal(DO);
					Sleep(500);
					DO[4] = 1;
					setVal(DO);
					Sleep(500);
					getBinaryNum(DI);
					prevDI0 = DI[0];
					if (DI[1]) break;
					for (int i = 0; i < 8; i++) {
						DO[i] = !DO[i];
						setVal(DO);
						Sleep(100);
						getBinaryNum(DI);
						prevDI0 = DI[0];
						if (DI[1]) break;
					}
					for (int i = 7; i >=0; i--) {
						DO[i] = !DO[i];
						setVal(DO);
						Sleep(100);
						getBinaryNum(DI);
						prevDI0 = DI[0];
						if (DI[1]) break;
					}
					DO[0] = 0;
					setVal(DO);
					Sleep(500);
					DO[7] = 0;
					setVal(DO);
					Sleep(500);
					getBinaryNum(DI);
					prevDI0 = DI[0];
					if (DI[1]) break;
					DO[1] = 0;
					setVal(DO);
					Sleep(500);
					DO[6] = 0;
					setVal(DO);
					Sleep(500);
					getBinaryNum(DI);
					prevDI0 = DI[0];
					if (DI[1]) break;
					DO[2] = 0;
					setVal(DO);
					Sleep(500);
					DO[5] = 0;
					setVal(DO);
					Sleep(500);
					getBinaryNum(DI);
					prevDI0 = DI[0];
					if (DI[1]) break;
					DO[3] = 0;
					setVal(DO);
					Sleep(500);
					DO[4] = 0;
					setVal(DO);
					Sleep(500);
					getBinaryNum(DI);
					prevDI0 = DI[0];
					if (DI[1]) break;
				}
			}
			//mode 2:Random!!!
			else if (!DI[2]) {
				int DO[8] = { 0 };
				setVal(DO);
				while (true) {
					getBinaryNum(DI);
					prevDI0 = DI[0];
					if (DI[2]) break;
					int pos = num_dis(gen);
					int time = time_dis(gen);
					DO[pos] = !DO[pos];
					setVal(DO);
					Sleep(time);
				}
			}
			/* mode 3:Zig-Zag
			 *  ------->
			 *   <-----
			 *    --->
			 *     <-
			 *    --->
			 *   <-----
			 *  ------->
			 */
			else if (!DI[3]) {
				int DO[8] = { 0 };
				int layer = 0;
				while (true)
				{
					getBinaryNum(DI);
					prevDI0 = DI[0];
					if (DI[3]) break;
					while (layer<4)
					{
						for (int i = layer; i <= 7 - layer; i++)
						{
							getBinaryNum(DI);
							prevDI0 = DI[0];
							if (DI[3]) break;
							DO[i] = 1;
							setVal(DO);
							Sleep(500);
						}
						for (int& i:DO)
							i = 0;
						layer++;
						getBinaryNum(DI);
						if (DI[3]) break;
						for (int i = 7 - layer; i >= layer; i--)
						{
							getBinaryNum(DI);
							prevDI0 = DI[0];
							if (DI[3]) break;
							DO[i] = 1;
							setVal(DO);
							Sleep(500);
						}
						for (int& i : DO)
							i = 0;
						layer++;
					}
					getBinaryNum(DI);
					prevDI0 = DI[0];
					if (DI[3]) break;
					layer = 2;
					for (int i = layer; i <= 7 - layer; i++)
					{
						getBinaryNum(DI);
						prevDI0 = DI[0];
						if (DI[3]) break;
						DO[i] = 1;
						setVal(DO);
						Sleep(500);
					}
					getBinaryNum(DI);
					prevDI0 = DI[0];
					if (DI[3]) break;
					for (int& i : DO)
						i = 0;
					layer--;
					for (int i = 7 - layer; i >= layer; i--)
					{
						getBinaryNum(DI);
						prevDI0 = DI[0];
						if (DI[3]) break;
						DO[i] = 1;
						setVal(DO);
						Sleep(500);
					}
					for (int& i : DO)i = 0;
					layer--;
				}
			}
			else if (!DI[4]){
				int DO[8] = { 0 };
				int count = 0;
				setVal(DO);
				while (true) {
					getBinaryNum(DI);
					prevDI0 = DI[0];
					if (DI[4]) break;
					for (int i = 0; i < 3; i++) {
						getBinaryNum(DI);
						prevDI0 = DI[0];
						if (DI[4]) break;
						DO[(count + i) % 8] = 1;
						setVal(DO);
						Sleep(200);
					}
					for (int i = 0; i < 8; i++) {
						DO[i] = 0;
						setVal(DO);
					}
					count = (count + 1) % 8;
				}
			}
		}
		else {
			int DO[8] = { 0 };
			setVal(DO);
		}
	}
	ShutdownWinIo();
}