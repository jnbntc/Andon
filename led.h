#include <cstring>
#include <thread>

using namespace std;

class Led {
	private:
		static const string exportPath;
		static const int blinkPeriod;
		string directionPath;
		string valuePath;
		int gpio;
		bool updateEnd;
		thread updateThread;
	public:
		Led(int gpio);
		void on();
		void off();
		void blink();
		void blink1();
		void blink2();
		void blink4();
		void blink8();
	private:
		void endUpdate();
		void onTask();
		void offTask();
		void blinkTask(int n);
};
