#include <windows.h>
#include <iostream>
#include "winio.h"
#include <conio.h>
#include <cstdlib>
#include <ctime>

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

class Lamp {
	int i;
	bool is_on;
	time_t start;
public:
	Lamp(int i) : i(i), is_on(false), start(0) {}
	void check_input(int input) {
		if (input) {
			if (!is_on) {
				is_on = true;
				cout << "楼层" << i + 1 << "检测到人，灯已点亮\n"; 
			}
			start = time(NULL);
		}
	}
	void update_state() {
		if (is_on && difftime(time(NULL), start) >= 10) {
			is_on = false;
			cout << "楼层" << i + 1 << "灯已熄灭\n";
		}
	}
	int output() const {
		return is_on ? 1 : 0;
	}
};

void main(void) {
	if (!InitializeWinIo())
	{
		cout << "Error In InitializeWinIo!" << endl;
		exit(1);
	}
	int DI[8] = { 1 };
	int DO[8] = { 0 };
	setVal(DO);
	Lamp lamps[8] = {
		Lamp(0), Lamp(1), Lamp(2), Lamp(3), Lamp(4), Lamp(5), Lamp(6), Lamp(7)
	};
	cout << "照明系统，启动！\n";
	while (1) {
		if (_kbhit()){
			break;
		}
		for (int i = 0; i < 8; i++) {
			getBinaryNum(DI);
			lamps[i].check_input(!DI[i]);
			lamps[i].update_state();
			DO[7 - i] = lamps[i].output();
		}
		setVal(DO);
		Sleep(100);
	}
	ShutdownWinIo();
}