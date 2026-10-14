#include <windows.h>
#include <iostream>
#include <conio.h>
#include "winio.h"

#pragma comment(lib,"winio.lib")
using namespace std;

float AD(int channel)
{
	unsigned short BASE_ADDRESS = 0x3F40;
	float fHiVolt = 10.0, fLoVolt = -10.0;
	unsigned short adData;
	unsigned char ucGain = 0x04;
	unsigned char ucStatus = 1;
	DWORD p;
	SetPortVal(BASE_ADDRESS + 9, 0, 1);
	SetPortVal(BASE_ADDRESS + 4, channel, 1);
	SetPortVal(BASE_ADDRESS + 5, channel, 1);
	SetPortVal(BASE_ADDRESS + 2, ucGain, 1);
	SetPortVal(BASE_ADDRESS + 6, 0x01, 1);
	SetPortVal(BASE_ADDRESS + 0, 0, 1);
	ucStatus = 1;
	while (ucStatus == 1)
	{
		GetPortVal(BASE_ADDRESS + 7, &p, 1);
		ucStatus = p & 0x01;
	}
	GetPortVal(BASE_ADDRESS + 0, &p, 2);
	adData = p & 0x0fff;
	return (fHiVolt - fLoVolt) * adData / 4095.0 + fLoVolt;
}

void DA(int channel, float vout)
{
	unsigned short BASE_ADDRESS = 0x3F40;
	float fHiVolt = 10, fLoVolt = 0;
	int AOPort = 10 + channel * 2;
	unsigned short outData;
	int LByte, HByte;
	SetPortVal(BASE_ADDRESS + 14, 1, 1);
	if (vout < fLoVolt) vout = fLoVolt;
	if (vout > fHiVolt) vout = fHiVolt;
	outData = (unsigned short)((vout - fLoVolt) / (fHiVolt - fLoVolt) * 0xfff);
	LByte = outData & 0xff;
	HByte = (outData >> 8) & 0x0f;
	SetPortVal(BASE_ADDRESS + AOPort, LByte, 1);
	SetPortVal(BASE_ADDRESS + AOPort + 1, HByte, 1);
}

int main()
{
	if (!InitializeWinIo())
	{
		cout << "Error In InitializeWinIo!" << endl;
		return 1;
	}

	int ao_channel, ai_channel;
	float set_voltage, read_voltage;
	float abs_error, rel_error;

	cout << "请输入DA输出通道号 (如0): ";
	cin >> ao_channel;
	cout << "请输入AD采集通道号 (如0): ";
	cin >> ai_channel;

	while (1)
	{
		cout << "请输入要输出的电压 (0~10V): ";
		cin >> set_voltage;

		// 输出
		DA(ao_channel, set_voltage);

		Sleep(100); // 等待稳定

		// 采集
		read_voltage = AD(ai_channel);

		// 误差
		abs_error = fabs(read_voltage - set_voltage);
		rel_error = (fabs(set_voltage) > 1e-6) ? (abs_error / fabs(set_voltage)) : 0;

		cout << "设定输出: " << set_voltage << " V" << endl;
		cout << "回读采集: " << read_voltage << " V" << endl;
		cout << "绝对误差: " << abs_error << " V" << endl;
		cout << "相对误差: " << rel_error * 100 << " %" << endl;

		cout << "按n/N继续，其它键退出..." << endl;
		char c = _getch();
		if (c != 'n' && c != 'N') break;
		system("cls");
	}

	// 清零输出
	DA(ao_channel, 0);
	ShutdownWinIo();
	return 0;
}
