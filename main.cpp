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

#include <iomanip>
#include <sstream>
#include <string>

#include "keymap.h"
#include "led.h"

using namespace std;

std::string inputDevPath_Unitech;
std::string inputDevPath_Datalogic;
std::string idScanner;
bool sendIDScanner;
bool logMessageFile;

queue<string> barcodes;
volatile int sockfd = -1;
struct sockaddr_in remote;
struct sockaddr_in local;

//llamo a la función para leer la ruta del archivo de configuración
void readConfigFile();
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

	readConfigFile(); // Se agrega la llamada a la función aquí.

	//std::cout << "inputDevPath_Datalogic: " << inputDevPath_Datalogic << std::endl;
	//std::cout << "inputDevPath_Unitech: " << inputDevPath_Unitech << std::endl;
	//std::cout << "idScanner: " << idScanner << std::endl;
	//std::cout << "sendIDScanner: " << sendIDScanner << std::endl;
  
	udpInit(argv[1], stoi(argv[2]));

	thread barcodeReaderThread(barcodeReaderTask);
  thread udpRecvThread(udpRecvTask);
	thread udpSendThread(udpSendTask);
	
 
	barcodeReaderThread.join();
  udpRecvThread.join();
	udpSendThread.join();
	

	return EXIT_SUCCESS;
}

// función para leer la ruta del scanner desde el archivo de configuración
void readConfigFile() {
    std::ifstream configFile("/home/pi/andon/config.ini"); // std::ifstream abre y lee el archivo config
    if (configFile.is_open()) {
        std::string line;
        while (std::getline(configFile, line)) {//std::getline lee cada linea del archivo config.
            size_t equals_pos = line.find('=');
            if (equals_pos != std::string::npos) {
                std::string variable_name = line.substr(0, equals_pos);
                if (variable_name == "inputDevPath_Datalogic") {
                    inputDevPath_Datalogic = line.substr(equals_pos + 1);
                } 
				else if (variable_name == "inputDevPath_Unitech") {
                    inputDevPath_Unitech = line.substr(equals_pos + 1);
                } 
				else if (variable_name == "idScanner") {
                    idScanner = line.substr(equals_pos + 1);
                }
				else if (variable_name == "sendIDScanner") {
					if (line.substr(equals_pos + 1) == "true") {
						sendIDScanner = true;
				} else {
						sendIDScanner = false;
						}
				}
				else if (variable_name == "logMessageFile") {
					if (line.substr(equals_pos + 1) == "true") {
						logMessageFile = true;
				} else {
						logMessageFile = false;
						}
				}
                // se pueden agregar mas condiciones para que lea otras variables
            }
        }
    }
    configFile.close();
}

//Funcion que verifica la pistola //LED VERDE Y ROJO
void barcodeReaderTask() { 
	int fd = -1;
	struct input_event event;
	bool lshift = false;
	bool rshift = false;
  //El led  ||blink = funciona|| ||.on .blink2 = Queda apagado|| ||.off = queda prendido||
	Led led0(14); // Se asigna LED VERDE
  Led led2(21); // Se asigna LED ROJO
  led0.blink();
  led2.on();
	
  std::cout << "LED VERDE encendido" << std::endl;
  
	string str = "";

	while (true) {
		ssize_t rd = read(fd, &event, sizeof(struct input_event));
		if (rd < 0) {
			if (fd >= 0) {
				ioctl(fd, EVIOCGRAB, (void *) 0);
				close(fd);
				fd = -1; // -1 significa que no hay ninguna lectora. Segun puerto USB tiene o 5 o 6
			}
			
      if (fd < 0){
      fd = open(inputDevPath_Unitech.c_str(), O_RDONLY); // Se asigna lectora Unitech
      std::cout << "Se asigna Lectora Unitech" << std::endl;
      std::cout << fd << std::endl;
        if(fd > 0){ //Si existe prende led
        led0.blink(); //LED VERDE
        led2.on(); //LED ROJO
        }
      
      }
      
			if (fd < 0){ //Si fd es menor se asigna la datalogic
				fd = open(inputDevPath_Datalogic.c_str(), O_RDONLY);
        std::cout << "No hay Lectora Unitech" << std::endl;
        std::cout << "Se asigna Lectora Datalogic" << std::endl;
        std::cout << fd << std::endl;
        if(fd > 0){ //Si existe prende led
          led0.blink(); //LED VERDE
          led2.on(); //LED ROJO
        }
        
        
        }
			if (fd < 0){ // luz roja
        std::cout << "No hay ninguna lectota Datalogic o Unitech conectada" << std::endl;
        led0.on();
        led2.blink();
				this_thread::sleep_for(chrono::seconds(3)); // Espera x segundos antes de arrancar el while
        }
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

// funcion cuando la pistola hace lectura // LED AMARILLO
void udpRecvTask() {
	Led led1(15); //LED AMARILLO 
	led1.on();   //Esto indica que inicialmente, el LED está apagado. Hay algo que invierte este metodo.
	char readBuffer[1024]; //Se declara un búfer de lectura llamado readBuffer de 1024 bytes para almacenar los datos recibidos a través de UDP.
 
	while (true) { //Se crea un Iterador
		memset(readBuffer, 0, sizeof(readBuffer)); //En cada iteración del bucle, se borra el contenido del búfer de lectura readBuffer utilizando memset
		ssize_t rlen = recv(sockfd, readBuffer, sizeof(readBuffer), 0); //Se utiliza recv para recibir datos a través de un socket UDP (sockfd) y se almacenan en readBuffer. La longitud        los datos recibidos se almacena en la variable rlen.
		if (rlen > 0) { //Se verifica si se han recibido datos (rlen > 0). Si se han recibido datos, se imprime el contenido de readBuffer en la consola con la etiqueta "received".
			cout << "received: " << readBuffer << endl;
      //Si la longitud de los datos recibidos (rlen) es mayor que 4 (esto se refiere a la cantidad de bytes esperados en el paquete UDP), se procesan los datos. En este caso, se espera         que el paquete contenga información sobre el estado del LED y se extraen tres secciones de datos del paquete (idBuffer, ledBuffer y stBuffer) utilizando strncpy.
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
				uint8_t st = atoi(stBuffer); //Se convierten los datos de stBuffer a un entero (st) utilizando atoi.
        
        //Se utiliza una estructura switch para realizar diferentes acciones en función del valor de st. Use el std:out para saber que opcion elige la lectura.
				switch (st) {
				case 0:
					led1.off();
          // std::cout << "opcion 0" << std::endl;
					break;
				case 1:
					led1.on(); // Estaba ON
          // std::cout << "opcion 1" << std::endl;
					break;
				case 2:
					led1.blink(); //Estaba blink
          // std::cout << "opcion 2" << std::endl;      
					break;
				case 3:
					led1.blink1();//Estaba blink1
          // std::cout << "opcion 3" << std::endl;
					break;
				case 4:
					led1.blink2();//Estaba blink2
         // std::cout << "opcion 4" << std::endl;
					break;
				case 5:
					led1.blink4();//Estaba blink4
          // std::cout << "opcion 5" << std::endl;
					break;
				case 6:
          // std::cout << "opcion 6" << std::endl; // Cuando lee usa este valor
					led1.blink8();//Estaba blink8 
          std::this_thread::sleep_for(std::chrono::seconds(1)); // Esto hace que despues del blink8 se corte 1 segundo el hilo del led para cambiar al proximo estado
          led1.on(); 
          
					break;
				default:
					led1.off();//Estaba blink
					break;
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

void logMessage(const string& message) {
  ofstream logFile;
  logFile.open("log.txt", ios::app);

  auto now = std::chrono::system_clock::now();
  auto in_time_t = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;
  ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:") << std::setfill('0') << std::setw(2) << std::localtime(&in_time_t)->tm_min << ":" << std::setfill('0') << std::setw(2) << std::localtime(&in_time_t)->tm_sec;

  logFile << "[" << ss.str() << "] " << message << endl;
  logFile.close();
}


void udpSendTask() {
  string str;
  while (true) {
    if (!barcodes.empty()) {
      str = barcodes.front();
      barcodes.pop();	  
      sendto(sockfd, str.c_str(), strlen(str.c_str()), 0, (struct sockaddr *) &remote, sizeof(remote));
      if (logMessageFile) {
          logMessage("Sent: " + str);
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
