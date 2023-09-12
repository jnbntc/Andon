#include <chrono>
#include <fstream>
#include <iostream>

#include "led.h"

const string Led::exportPath = "/sys/class/gpio/export";
const int Led::blinkPeriod = 200;

Led::Led(int gpio) : gpio(gpio) {
	updateEnd = false;
	directionPath = "/sys/class/gpio/gpio" + to_string(gpio) + "/direction";
	valuePath = "/sys/class/gpio/gpio" + to_string(gpio) + "/value";
	this_thread::sleep_for(chrono::milliseconds(100));
	ofstream exportfs(Led::exportPath.c_str());
	exportfs << gpio;
	exportfs.close();
	this_thread::sleep_for(chrono::milliseconds(100));
	ofstream gpiodirfs(directionPath.c_str());
	gpiodirfs << "out";
	gpiodirfs.close();
}

void Led::on() {
	endUpdate();
	updateThread = thread([=] { onTask(); });
}

void Led::off() {
	endUpdate();
	updateThread = thread([=] { offTask(); });
}

void Led::blink() {
	endUpdate();
	updateThread = thread([=] { blinkTask(-1); });
}

void Led::blink1() {
	endUpdate();
	updateThread = thread([=] { blinkTask(1); });
}

void Led::blink2() {
	endUpdate();
	updateThread = thread([=] { blinkTask(2); });
}

void Led::blink4() {
	endUpdate();
	updateThread = thread([=] { blinkTask(4); });
}

void Led::blink8() {
	endUpdate();
	updateThread = thread([=] { blinkTask(8); });
}

void Led::endUpdate() {
	updateEnd = true;
	if (updateThread.joinable()) updateThread.join();
	updateEnd = false;
}

void Led::onTask() {
	ofstream gpiovalfs(valuePath.c_str());
	gpiovalfs << 1;
	gpiovalfs.flush();
	while (!updateEnd) { }
	gpiovalfs.close();
}

void Led::offTask() {
	ofstream gpiovalfs(valuePath.c_str());
	gpiovalfs << 0;
	gpiovalfs.flush();
	while (!updateEnd) { }
	gpiovalfs.close();
}

void Led::blinkTask(int n) {
	ofstream gpiovalfs(valuePath.c_str());
	while (!updateEnd && (n == -1 || n > 0)) {
		gpiovalfs << 1;
		gpiovalfs.flush();
		this_thread::sleep_for(chrono::milliseconds(blinkPeriod / 2));
		gpiovalfs << 0;
		gpiovalfs.flush();
		this_thread::sleep_for(chrono::milliseconds(blinkPeriod / 2));
		if (n > 0)
			n--;
	}
	gpiovalfs.close();
}
