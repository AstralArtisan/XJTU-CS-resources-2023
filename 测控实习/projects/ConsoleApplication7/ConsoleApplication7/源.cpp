#include <windows.h>
#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include "winio.h"
#include <conio.h>
#pragma comment(lib,"winio.lib")

using namespace std;

unsigned short BASE_ADDRESS = 0x3F40;
int Port = 16;

std::atomic<bool> power_on(false);
std::atomic<bool> running(false);
std::atomic<bool> paused(false);
std::atomic<bool> water_full(false);
std::atomic<bool> stop_requested(false);

// 用于检测按键边沿
bool last_power = false, last_start = false, last_pause = false;


// 将DO[7]-DO[0]按位输出
void set_leds(const int DO[8]) {
	SetPortVal(BASE_ADDRESS + Port, [&]() {
		int v = 0;
		for (int i = 7; i >= 0; --i)
			v = (v << 1) | (DO[i] ? 1 : 0); // DO[7]为高位
		return v;
	}(), 1);
}

// 清空所有LED
void led_off_all() {
	int DO[8] = { 0 };
	set_leds(DO);
}

// 指定LED闪烁
void led_blink(int idx, bool fast = false, int total_ms = 300) {
	int DO[8] = { 0 };
	int delay = fast ? 100 : 300;
	int t = 0;
	while (t < total_ms) {
		DO[idx] = 1;
		set_leds(DO);
		Sleep(delay / 2);
		DO[idx] = 0;
		set_leds(DO);
		Sleep(delay / 2);
		t += delay;
		// 响应暂停/电源
		if (!power_on || paused || stop_requested) return;
	}
}

// 顺序点亮LED流水灯
void led_seq(bool fast = false, int total_ms = 1200) {
	int DO[8] = { 0 };
	int delay = fast ? 50 : 200;
	int loop = total_ms / (delay * 6);
	for (int l = 0; l <= loop; ++l) {
		for (int i = 0; i < 6; i++) {
			for (int j = 0; j < 8; j++) { DO[j] = 0; }
			DO[i] = 1;
			set_leds(DO);
			Sleep(delay);
			// 响应暂停/电源
			if (!power_on || paused || stop_requested) return;
		}
	}
}

// 读取DI输入，更新全局变量
void input_thread_func() {
	while (!stop_requested) {
		int DI[8] = { 0 };
		DWORD di_data;
		GetPortVal(BASE_ADDRESS + Port, &di_data, 1);
		for (int i = 0; i < 8; i++)
			DI[i] = (di_data >> i) & 0x1;
		// DI[0] 电源，DI[1] 启动/暂停，DI[2] 液位传感器（低有效），
		bool curr_power = !DI[0];
		bool curr_start = !DI[1];
		bool curr_water = !DI[2];

		// 电源检测
		if (curr_power && !last_power) {
			power_on = true;
			cout << "电源打开" << endl;
		}
		if (!curr_power && last_power) {
			power_on = false;
			running = false;
			paused = false;
			cout << "电源关闭" << endl;
		}
		last_power = curr_power;

		// 启动/暂停（边沿翻转，防抖）
		if (curr_start && !last_start) {
			if (!running) {
				paused = false;
				running = true; // 启动
				cout << "启动" << endl;
			}
			else {
				running = false;
				paused = true;        // 已启动则为暂停/恢复
				cout << "暂停" << endl;
			}
		}
		last_start = curr_start;

		// 液位传感器
		water_full = curr_water;

		Sleep(100);
	}
}

bool wait_with_check(int ms, int led_mode, bool fast = false) {
	int step = 1000;
	for (int t = 0; t < ms; t += step) {
		if (!power_on) return false;
		while (paused && power_on) {
			Sleep(50);
			if (!power_on) return false;
		}
		// 选择LED动作
		switch (led_mode) {
		case 1: led_blink(0, fast, step); break; // LED1
		case 2: led_blink(1, fast, step); break; // LED2
		case 3: led_blink(2, fast, step); break; // LED3
		case 6: led_blink(5, fast, step); break; // LED6
		case 9: led_seq(fast, step); break;      // 流水灯
		default: led_off_all(); break;
		}
		Sleep(step);  // 延时
		if (!power_on || stop_requested) return false;
	}
	return true;
}


// 主控流程线程
void washer_main() {
	enum STATE {
		IDLE, FILL1, SOAK, WASH, DRAIN1, SPIN1,
		FILL2, RINSE1, DRAIN2, SPIN2,
		FILL3, RINSE2, DRAIN3, SPIN3,
		DONE
		//IDLE等待，FILL注水，SOAK浸泡，WASH洗涤，DRAIN排水，SPIN脱水，RINSE漂洗，DONE完成
	};
	STATE state = IDLE;

	while (!stop_requested) {
		if (!power_on) {
			led_off_all();
			state = IDLE;
			running = false;
			paused = false;
			Sleep(100);
			continue;
		}
		if (!running) {
			led_off_all();
			state = IDLE;
			Sleep(100);
			continue;
		}

		switch (state) {
		case IDLE:
			led_off_all();
			if (running) state = FILL1;
			break;
		case FILL1:
			// 第一次注水
			cout << "注水\n";
			while (!water_full && power_on && running && !paused && !stop_requested) {
				led_blink(0, true, 200);
			}
			if (!power_on || !running) { state = IDLE; break; }
			state = SOAK;
			break;
		case SOAK:
			// 浸泡 5秒
			cout << "浸泡\n";
			if (!wait_with_check(5 * 1000, 2, true)) { state = IDLE; break; }
			state = WASH;
			break;
		case WASH:
			// 洗涤 5秒 慢流水灯
			cout << "洗涤\n";
			if (!wait_with_check(5 * 1000, 9)) { state = IDLE; break; }
			state = DRAIN1;
			break;
		case DRAIN1:
			// 排水 4秒
			cout << "排水\n";
			if (!wait_with_check(4 * 1000, 3, true)) { state = IDLE; break; }
			state = SPIN1;
			break;
		case SPIN1:
			// 脱水 3秒 快流水灯
			cout << "脱水\n";
			if (!wait_with_check(3 * 1000, 9, true)) { state = IDLE; break; }
			state = FILL2;
			break;

			// 第一次漂洗
		case FILL2:
			// 注水
			cout << "注水\n";
			while (!water_full && power_on && running && !paused && !stop_requested) {
				led_blink(0, true, 200);
			}
			if (!power_on || !running) { state = IDLE; break; }
			state = RINSE1;
			break;
		case RINSE1:
			// 漂洗 4秒 慢流水灯
			cout << "漂洗\n";
			if (!wait_with_check(4 * 1000, 9)) { state = IDLE; break; }
			state = DRAIN2;
			break;
		case DRAIN2:
			// 排水 4秒
			cout << "排水\n";
			if (!wait_with_check(4 * 1000, 3, true)) { state = IDLE; break; }
			state = SPIN2;
			break;
		case SPIN2:
			// 脱水 3秒 快流水灯
			cout << "脱水\n";
			if (!wait_with_check(3 * 1000, 9, true)) { state = IDLE; break; }
			state = FILL3;
			break;

			// 第二次漂洗
		case FILL3:
			// 注水
			cout << "注水\n";
			while (!water_full && power_on && running && !paused && !stop_requested) {
				led_blink(0, true, 200);
			}
			if (!power_on || !running) { state = IDLE; break; }
			state = RINSE2;
			break;
		case RINSE2:
			// 漂洗 4秒 慢流水灯
			cout << "漂洗\n";
			if (!wait_with_check(4 * 1000, 9)) { state = IDLE; break; }
			state = DRAIN3;
			break;
		case DRAIN3:
			// 排水 4秒
			cout << "排水\n";
			if (!wait_with_check(4 * 1000, 3, true)) { state = IDLE; break; }
			state = SPIN3;
			break;
		case SPIN3:
			// 脱水 3秒 快流水灯
			cout << "脱水\n";
			if (!wait_with_check(3 * 1000, 9, true)) { state = IDLE; break; }
			state = DONE;
			break;

		case DONE:
			// LED6闪烁
			cout << "完成！\n";
			for (int i = 0; i < 20 && power_on && running && !paused && !stop_requested; ++i) {
				led_blink(5, true, 250);
			}
			state = IDLE;
			running = false;
			paused = false;
			break;
		}
	}
}


int main() {
	if (!InitializeWinIo()) {
		cout << "Error In InitializeWinIo!" << endl;
		return 1;
	}

	stop_requested = false;
	thread input_thread(input_thread_func);
	thread washer_thread(washer_main);

	input_thread.join();
	washer_thread.join();
	led_off_all();
	ShutdownWinIo();
	return 0;
}
