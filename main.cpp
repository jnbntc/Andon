#include <arpa/inet.h>
#include <cctype>
#include <csignal>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <fcntl.h>
#include <linux/input.h>
#include <netdb.h>
#include <netinet/in.h>
#include <queue>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>

#include "keymap.h"
#include "led.h"

using namespace std;

const string inputDevPath_Unitech = "/dev/input/by-id/usb-_HID_Keyboard_1.000-event-kbd";
const string inputDevPath_Datalogic = "/dev/input/by-id/usb-Datalogic_ADC__Inc._Handheld_Barcode_Scanner_S_N_G17M22141-event-kbd";

queue<string> barcodes;
volatile int sockfd = -1;
struct sockaddr_in remote;
struct sockaddr_in local;

void barcodeReaderTask();
void udpInit(char *serverIp, int serverPort);
void udpSendTask();
void udpRecvTask();
char getChar(__u16 code, bool shift);

int main(int argc, char *argv[]) {
	if (argc != 3) {
		cout << "Usage: andon <serverip> <serverport>" << endl;
		return EXIT_SUCCESS;
	}

	udpInit(argv[1], stoi(argv[2]));

	thread barcodeReaderThread(barcodeReaderTask);
	thread udpSendThread(udpSendTask);
	thread udpRecvThread(udpRecvTask);

	barcodeReaderThread.join();
	udpSendThread.join();
	udpRecvThread.join();

	return EXIT_SUCCESS;
}

void barcodeReaderTask() {
	int fd = -1;
	struct input_event event;
	bool lshift = false;
	bool rshift = false;

	Led led0(14);
	led0.blink();

	string str = "";

	while (true) {
		ssize_t rd = read(fd, &event, sizeof(struct input_event));
		if (rd < 0) {
			if (fd >= 0) {
				ioctl(fd, EVIOCGRAB, (void *) 0);
				close(fd);
				fd = -1;
			}
			fd = open(inputDevPath_Unitech.c_str(), O_RDONLY);
			if (fd < 0)
				fd = open(inputDevPath_Datalogic.c_str(), O_RDONLY);
			if (fd < 0)
				this_thread::sleep_for(chrono::seconds(1));
			else
				ioctl(fd, EVIOCGRAB, (void *) 1);
		} else if (rd >= (ssize_t) sizeof(struct input_event)) {
			if (event.type == EV_KEY) {
				if (event.code == KEY_LEFTSHIFT)
					lshift = event.value == 1;
				else if (event.code == KEY_RIGHTSHIFT)
					rshift = event.value == 1;
				else if (event.value == 1) {
					char c = getChar(event.code, lshift | rshift);
					if (c == 0x00)
						continue;
					str += c;
					if (c == '\r' || c == '\n') {
						if (str.length() > 1) {
							cout << str << endl;
							barcodes.push(str);
						}
						str = "";
					}
				}
			}
		}
	}
}

void udpInit(char *serverIp, int serverPort) {
	memset(&remote, 0, sizeof(struct sockaddr_in));
	remote.sin_family = AF_INET;
	remote.sin_addr.s_addr = inet_addr(serverIp);
	remote.sin_port = htons(serverPort);

	int localPort = serverPort;

	memset(&local, 0, sizeof(struct sockaddr_in));
	local.sin_family = AF_INET;
	local.sin_addr.s_addr = htonl(INADDR_ANY);
	local.sin_port = htons(localPort);

	signal(SIGPIPE, SIG_IGN);

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);

	bind(sockfd, (struct sockaddr *) &local, sizeof(local));
}

void udpSendTask() {
	string str;
	while (true) {
		if (!barcodes.empty()) {
			str = barcodes.front();
			barcodes.pop();
			sendto(sockfd, str.c_str(), strlen(str.c_str()), 0, (struct sockaddr *) &remote, sizeof(remote));
		}
	}
}

void udpRecvTask() {
	Led led1(15);
	led1.off();
	char readBuffer[1024];
	while (true) {
		memset(readBuffer, 0, sizeof(readBuffer));
		ssize_t rlen = recv(sockfd, readBuffer, sizeof(readBuffer), 0);
		if (rlen > 0) {
			cout << "received: " << readBuffer << endl;
			if (rlen > 4) {
				int i = 0;
				char idBuffer[4] = { };
				char ledBuffer[2] = { };
				char stBuffer[2] = { };
				strncpy(idBuffer, readBuffer + i, sizeof(idBuffer) - 1);
				i += sizeof(idBuffer) - 1;
				strncpy(ledBuffer, readBuffer + i, sizeof(ledBuffer) - 1);
				i += sizeof(ledBuffer) - 1;
				strncpy(stBuffer, readBuffer + i, sizeof(stBuffer) - 1);
				i += sizeof(stBuffer) - 1;
//				uint8_t id = atoi(idBuffer);
//				uint8_t led = atoi(ledBuffer);
				uint8_t st = atoi(stBuffer);
				switch (st) {
				case 0:
					led1.off();
					break;
				case 1:
					led1.on();
					break;
				case 2:
					led1.blink();
					break;
				case 3:
					led1.blink1();
					break;
				case 4:
					led1.blink2();
					break;
				case 5:
					led1.blink4();
					break;
				case 6:
					led1.blink8();
					break;
				default:
					led1.off();
					break;
				}
			}
		}
	}
}

char getChar(__u16 code, bool shift) {
	uint8_t idx = code & 0xff;
	char c = keyMap[idx];
	if (isprint(c)) {
		if (isalpha(c))
			return shift ? toupper(c) : c;
		return c;
	}
	if (iscntrl(c))
		return c;
	return 0x00;
}
