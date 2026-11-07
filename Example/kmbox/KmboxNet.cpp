

#include "KmboxNet.h"
#include "HidTable.h"
#include <time.h>
#include <iostream>
#include <iomanip>
#include <WS2tcpip.h>
#define monitor_begin 1
#define monitor_ok    2
#define monitor_exit  0

SOCKET sockClientfd = 0;               // Socket handle for client communication (keyboard and mouse network)
SOCKET sockMonitorfd = 0;              // Socket handle for monitoring communication
client_tx tx;                          // Data to send
client_tx rx;                          // Data received
SOCKADDR_IN addrSrv;
soft_mouse_t    softmouse;             // Software mouse data
soft_keyboard_t softkeyboard;          // Software keyboard data
static int monitor_run = 0;            // Indicates if physical keyboard and mouse monitoring is running
static int mask_keyboard_mouse_flag = 0; // Keyboard and mouse blocking status
static short monitor_port = 0;         // Monitoring port



#pragma pack(1)
typedef struct {
	unsigned char report_id;
	unsigned char buttons;      // 8 buttons available
	short x;                    // -32767 to 32767 (horizontal movement)
	short y;                    // -32767 to 32767 (vertical movement)
	short wheel;                // -32767 to 32767 (scroll wheel movement)
} standard_mouse_report_t;

typedef struct {
	unsigned char report_id;
	unsigned char buttons;      // 8 control buttons
	unsigned char data[10];     // Regular keys
} standard_keyboard_report_t;
#pragma pack()

standard_mouse_report_t     hw_mouse;   // Hardware mouse message
standard_keyboard_report_t  hw_keyboard; // Hardware keyboard message

// Generates a random number between A and B
int myrand(int a, int b) {
	int min = a < b ? a : b;
	int max = a > b ? a : b;
	return ((rand() % (max - min)) + min);
}

unsigned int StrToHex(char* pbSrc, int nLen)
{
	char h1, h2;
	unsigned char s1, s2;
	int i;
	unsigned int pbDest[16] = { 0 };
	for (i = 0; i < nLen; i++) {
		h1 = pbSrc[2 * i];
		h2 = pbSrc[2 * i + 1];
		s1 = toupper(h1) - 0x30;
		if (s1 > 9)
			s1 -= 7;
		s2 = toupper(h2) - 0x30;
		if (s2 > 9)
			s2 -= 7;
		pbDest[i] = s1 * 16 + s2;
	}
	return pbDest[0] << 24 | pbDest[1] << 16 | pbDest[2] << 8 | pbDest[3];
}

int NetRxReturnHandle(client_tx* rx, client_tx* tx) {   // Handles received content
	if (rx->head.cmd != tx->head.cmd)
		return err_net_cmd;  // Error: Command code mismatch
	if (rx->head.indexpts != tx->head.indexpts)
		return err_net_pts;  // Error: Timestamp mismatch
	return 0;                // No errors, return 0
	// return rx->head.rand;  // Actual return value (commented out)
}



/*
Connects to the KMBoxNet device. The input parameters are as follows:
ip   : The IP address of the device (displayed on the screen, e.g., 192.168.2.88)
port : Communication port number (displayed on the screen, e.g., 6234)
mac  : The MAC address of the device (displayed on the screen, e.g., 12345)
Return value: 0 for success, non-zero values indicate errors (refer to error codes)
*/
int kmNet_init(char* ip, char* port, char* mac) {
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;

	wVersionRequested = MAKEWORD(1, 1);
	err = WSAStartup(wVersionRequested, &wsaData);

	if (err != 0)
		return err_creat_socket;
	if (LOBYTE(wsaData.wVersion) != 1 || HIBYTE(wsaData.wVersion) != 1) {
		WSACleanup();
		sockClientfd = -1;
		return err_net_version;
	}

	srand((unsigned)time(NULL));
	sockClientfd = socket(AF_INET, SOCK_DGRAM, 0);
	addrSrv.sin_addr.S_un.S_addr = inet_addr(ip);
	addrSrv.sin_family = AF_INET;
	addrSrv.sin_port = htons(atoi(port)); // Port UUID[1] >> 16 (high 16 bits)

	tx.head.mac = StrToHex(mac, 4);       // MAC address of the device, fixed UUID[1]
	tx.head.rand = rand();                // Random value for potential data encryption in future
	tx.head.indexpts = 0;                 // Command counter
	tx.head.cmd = cmd_connect;            // Command type

	memset(&softmouse, 0, sizeof(softmouse));        // Clear software mouse data
	memset(&softkeyboard, 0, sizeof(softkeyboard));  // Clear software keyboard data

	err = sendto(sockClientfd, (const char*)&tx, sizeof(cmd_head_t), 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));
	Sleep(20); // Initial connection may take longer

	int clen = sizeof(addrSrv);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&addrSrv, &clen);
	if (err < 0)
		return err_net_rx_timeout;

	return NetRxReturnHandle(&rx, &tx);
}


/*
Move the mouse by x, y units. Moves in a single step with no trajectory simulation, at the fastest speed.
Use this function when writing your own trajectory movement.
Return value: 0 for normal execution, other values for errors.
*/
int kmNet_mouse_move(short x, short y) {
	int err;
	if (sockClientfd <= 0)     return err_creat_socket;

	tx.head.indexpts++;              // Command counter
	tx.head.cmd = cmd_mouse_move;    // Command
	tx.head.rand = rand();           // Random obfuscation value

	softmouse.x = x;
	softmouse.y = y;
	memcpy(&tx.cmd_mouse, &softmouse, sizeof(soft_mouse_t));

	int length = sizeof(cmd_head_t) + sizeof(soft_mouse_t);
	sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));

	softmouse.x = 0;
	softmouse.y = 0;

	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);
	if (err < 0)
		return err_net_rx_timeout;

	return NetRxReturnHandle(&rx, &tx);
}


/*
Mouse left button control
isdown : 0 for release, 1 for press
Return value: 0 for normal execution, other values for errors.
*/
int kmNet_mouse_left(int isdown) {
	int err;
	if (sockClientfd <= 0)     return err_creat_socket;

	tx.head.indexpts++;              // Command counter
	tx.head.cmd = cmd_mouse_left;    // Command
	tx.head.rand = rand();           // Random obfuscation value

	softmouse.button = (isdown ? (softmouse.button | 0x01) : (softmouse.button & (~0x01)));
	memcpy(&tx.cmd_mouse, &softmouse, sizeof(soft_mouse_t));

	int length = sizeof(cmd_head_t) + sizeof(soft_mouse_t);
	sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));

	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);
	if (err < 0)
		return err_net_rx_timeout;

	return NetRxReturnHandle(&rx, &tx);
}

/*
Mouse middle button control
isdown : 0 for release, 1 for press
Return value: 0 for normal execution, other values for errors.
*/
int kmNet_mouse_middle(int isdown) {
	int err;
	if (sockClientfd <= 0)     return err_creat_socket;

	tx.head.indexpts++;              // Command counter
	tx.head.cmd = cmd_mouse_middle;  // Command
	tx.head.rand = rand();           // Random obfuscation value

	softmouse.button = (isdown ? (softmouse.button | 0x04) : (softmouse.button & (~0x04)));
	memcpy(&tx.cmd_mouse, &softmouse, sizeof(soft_mouse_t));

	int length = sizeof(cmd_head_t) + sizeof(soft_mouse_t);
	sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));

	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);
	if (err < 0)
		return err_net_rx_timeout;

	return NetRxReturnHandle(&rx, &tx);
}

/*
Mouse right button control
isdown : 0 for release, 1 for press
Return value: 0 for normal execution, other values for errors.
*/
int kmNet_mouse_right(int isdown) {
	int err;
	if (sockClientfd <= 0)     return err_creat_socket;

	tx.head.indexpts++;              // Command counter
	tx.head.cmd = cmd_mouse_right;   // Command
	tx.head.rand = rand();           // Random obfuscation value

	softmouse.button = (isdown ? (softmouse.button | 0x02) : (softmouse.button & (~0x02)));
	memcpy(&tx.cmd_mouse, &softmouse, sizeof(soft_mouse_t));

	int length = sizeof(cmd_head_t) + sizeof(soft_mouse_t);
	sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));

	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);
	if (err < 0)
		return err_net_rx_timeout;

	return NetRxReturnHandle(&rx, &tx);
}

// Mouse wheel control
int kmNet_mouse_wheel(int wheel) {
	int err;
	if (sockClientfd <= 0)     return err_creat_socket;

	tx.head.indexpts++;              // Command counter
	tx.head.cmd = cmd_mouse_wheel;   // Command
	tx.head.rand = rand();           // Random obfuscation value

	softmouse.wheel = wheel;
	memcpy(&tx.cmd_mouse, &softmouse, sizeof(soft_mouse_t));

	int length = sizeof(cmd_head_t) + sizeof(soft_mouse_t);
	softmouse.wheel = 0;

	sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));

	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);
	if (err < 0)
		return err_net_rx_timeout;

	return NetRxReturnHandle(&rx, &tx);
}


/*
Full mouse report control function
*/
int kmNet_mouse_all(int button, int x, int y, int wheel) {
	int err;
	if (sockClientfd <= 0)     return err_creat_socket;

	tx.head.indexpts++;              // Command counter
	tx.head.cmd = cmd_mouse_wheel;   // Command
	tx.head.rand = rand();           // Random obfuscation value

	softmouse.button = button;
	softmouse.x = x;
	softmouse.y = y;
	softmouse.wheel = wheel;

	memcpy(&tx.cmd_mouse, &softmouse, sizeof(soft_mouse_t));

	int length = sizeof(cmd_head_t) + sizeof(soft_mouse_t);
	softmouse.x = 0;
	softmouse.y = 0;
	softmouse.wheel = 0;

	sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));

	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);
	if (err < 0)
		return err_net_rx_timeout;

	return NetRxReturnHandle(&rx, &tx);
}

/*
Mouse moves x, y units. Simulates human-like movement of x, y units. No mouse/keyboard anomaly detection.
If you haven't implemented curve-based movement, it's recommended to use this function. It avoids jumpy behavior, moving in small steps to reach the target point.
This function takes more time than kmNet_mouse_move.
'ms' sets how many milliseconds the movement should take. Note that don't set ms too small, as this can still cause mouse/keyboard data anomalies.
Try to make the movement look like human interaction. Actual time may be less than ms.
*/
int kmNet_mouse_move_auto(int x, int y, int ms)
{
	int err;
	if (sockClientfd <= 0)     return err_creat_socket;

	tx.head.indexpts++;                 // Command counter
	tx.head.cmd = cmd_mouse_automove;   // Command
	tx.head.rand = ms;                  // Random obfuscation value

	softmouse.x = x;
	softmouse.y = y;
	memcpy(&tx.cmd_mouse, &softmouse, sizeof(soft_mouse_t));

	int length = sizeof(cmd_head_t) + sizeof(soft_mouse_t);
	softmouse.x = 0;                    // Reset
	softmouse.y = 0;                    // Reset

	sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));

	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);
	if (err < 0)
		return err_net_rx_timeout;

	return NetRxReturnHandle(&rx, &tx);
}



/*
Second-order Bézier curve control
x, y    : Target point coordinates
ms      : Time to fit this process (in milliseconds)
x1, y1  : Control point p1 coordinates
x2, y2  : Control point p2 coordinates
*/
int kmNet_mouse_move_beizer(int x, int y, int ms, int x1, int y1, int x2, int y2)
{
	int err;
	if (sockClientfd <= 0)     return err_creat_socket;

	tx.head.indexpts++;                 // Command counter
	tx.head.cmd = cmd_bazerMove;        // Command
	tx.head.rand = ms;                  // Random obfuscation value

	softmouse.x = x;
	softmouse.y = y;
	softmouse.point[0] = x1;
	softmouse.point[1] = y1;
	softmouse.point[2] = x2;
	softmouse.point[3] = y2;

	memcpy(&tx.cmd_mouse, &softmouse, sizeof(soft_mouse_t));

	int length = sizeof(cmd_head_t) + sizeof(soft_mouse_t);
	softmouse.x = 0;                    // Reset
	softmouse.y = 0;                    // Reset

	sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));

	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);
	if (err < 0)
		return err_net_rx_timeout;

	return NetRxReturnHandle(&rx, &tx);
}


int kmNet_keydown(int vk_key)
{
	int i;
	if (vk_key >= KEY_LEFTCONTROL && vk_key <= KEY_RIGHT_GUI) // Control keys
	{
		switch (vk_key)
		{
		case KEY_LEFTCONTROL: softkeyboard.ctrl |= BIT0; break;
		case KEY_LEFTSHIFT:   softkeyboard.ctrl |= BIT1; break;
		case KEY_LEFTALT:     softkeyboard.ctrl |= BIT2; break;
		case KEY_LEFT_GUI:    softkeyboard.ctrl |= BIT3; break;
		case KEY_RIGHTCONTROL:softkeyboard.ctrl |= BIT4; break;
		case KEY_RIGHTSHIFT:  softkeyboard.ctrl |= BIT5; break;
		case KEY_RIGHTALT:    softkeyboard.ctrl |= BIT6; break;
		case KEY_RIGHT_GUI:   softkeyboard.ctrl |= BIT7; break;
		}
	}
	else
	{ // Regular keys  
		for (i = 0; i < 10; i++) // First, check if vk_key exists in the queue
		{
			if (softkeyboard.button[i] == vk_key)
				goto KM_down_send; // vk_key already exists in the queue, just send it
		}
		// vk_key doesn't exist in the queue
		for (i = 0; i < 10; i++) // Traverse all data, add vk_key to the queue
		{
			if (softkeyboard.button[i] == 0)
			{ // If there's space in the queue, add vk_key
				softkeyboard.button[i] = vk_key;
				goto KM_down_send;
			}
		}
		// Queue is full, remove the oldest entry
		memcpy(&softkeyboard.button[0], &softkeyboard.button[1], 10);
		softkeyboard.button[9] = vk_key;
	}
KM_down_send:
	int err;
	if (sockClientfd <= 0)     return err_creat_socket;
	tx.head.indexpts++;                 // Command counter
	tx.head.cmd = cmd_keyboard_all;     // Command
	tx.head.rand = rand();              // Random obfuscation value

	memcpy(&tx.cmd_keyboard, &softkeyboard, sizeof(soft_keyboard_t));

	int length = sizeof(cmd_head_t) + sizeof(soft_keyboard_t);
	sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));

	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);
	if (err < 0)
		return err_net_rx_timeout;

	return NetRxReturnHandle(&rx, &tx);
}


int kmNet_keyup(int vk_key)
{
	int i;
	if (vk_key >= KEY_LEFTCONTROL && vk_key <= KEY_RIGHT_GUI) // Control keys
	{
		switch (vk_key)
		{
		case KEY_LEFTCONTROL: softkeyboard.ctrl &= ~BIT0; break;
		case KEY_LEFTSHIFT:   softkeyboard.ctrl &= ~BIT1; break;
		case KEY_LEFTALT:     softkeyboard.ctrl &= ~BIT2; break;
		case KEY_LEFT_GUI:    softkeyboard.ctrl &= ~BIT3; break;
		case KEY_RIGHTCONTROL:softkeyboard.ctrl &= ~BIT4; break;
		case KEY_RIGHTSHIFT:  softkeyboard.ctrl &= ~BIT5; break;
		case KEY_RIGHTALT:    softkeyboard.ctrl &= ~BIT6; break;
		case KEY_RIGHT_GUI:   softkeyboard.ctrl &= ~BIT7; break;
		}
	}
	else
	{ // Regular keys  
		for (i = 0; i < 10; i++) // First, check if vk_key exists in the queue
		{
			if (softkeyboard.button[i] == vk_key) // vk_key already exists in the queue
			{
				memcpy(&softkeyboard.button[i], &softkeyboard.button[i + 1], 10 - i);
				softkeyboard.button[9] = 0;
				goto KM_up_send;
			}
		}
	}
KM_up_send:
	int err;
	if (sockClientfd <= 0)     return err_creat_socket;
	tx.head.indexpts++;                 // Command counter
	tx.head.cmd = cmd_keyboard_all;     // Command
	tx.head.rand = rand();              // Random obfuscation value

	memcpy(&tx.cmd_keyboard, &softkeyboard, sizeof(soft_keyboard_t));

	int length = sizeof(cmd_head_t) + sizeof(soft_keyboard_t);
	sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));

	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);
	if (err < 0)
		return err_net_rx_timeout;

	return NetRxReturnHandle(&rx, &tx);
}

// Reboot the box
int kmNet_reboot(void)
{
	int err;
	if (sockClientfd <= 0)     return err_creat_socket;
	tx.head.indexpts++;                // Command counter
	tx.head.cmd = cmd_reboot;          // Command
	tx.head.rand = rand();             // Random obfuscation value

	int length = sizeof(cmd_head_t);
	sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));

	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);
	WSACleanup();
	sockClientfd = -1;
	if (err < 0)
		return err_net_rx_timeout;

	return NetRxReturnHandle(&rx, &tx);
}

// Listen for physical keyboard and mouse events
static HANDLE handle_listen = NULL;
DWORD WINAPI ThreadListenProcess(LPVOID lpParameter)
{
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);            // Initialize Winsock, using UDP protocol
	SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);    // Create socket
	sockaddr_in servAddr;
	memset(&servAddr, 0, sizeof(servAddr));          // Fill each byte with 0
	servAddr.sin_family = PF_INET;                   // Use IPv4 address
	servAddr.sin_addr.s_addr = htonl(INADDR_ANY);    // Automatically use any available IP address
	servAddr.sin_port = htons(addrSrv.sin_port + 1); // Use the port number + 1
	bind(sock, (SOCKADDR*)&servAddr, sizeof(SOCKADDR));

	SOCKADDR cliAddr;  // Client address information
	int nSize = sizeof(SOCKADDR);
	char buff[1024];   // Buffer

	monitor_run = monitor_ok;
	while (monitor_run) {
		int strLen = recvfrom(sock, buff, 1024, 0, &cliAddr, &nSize);
		memcpy(&hw_mouse, buff, sizeof(hw_mouse));                            // Physical mouse state
		memcpy(&hw_keyboard, &buff[sizeof(hw_mouse)], sizeof(hw_keyboard));  // Physical keyboard state
	}

	closesocket(sock);
	return 0;
}

// Enable or disable keyboard and mouse monitoring
int kmNet_monitor(int enable)
{
	int err;
	if (sockClientfd <= 0)    return err_creat_socket;

	tx.head.indexpts++;                // Command index increment
	tx.head.cmd = cmd_monitor;         // Command for monitoring

	if (enable) {
		tx.head.rand = (addrSrv.sin_port + 1) | (0xaa55 << 16); // Random value for enabling monitoring
	}
	else {
		tx.head.rand = 0;              // Reset random value for disabling monitoring
	}

	int length = sizeof(cmd_head_t);
	sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));

	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);

	if (enable) {  // Enable monitoring
		do {
			if (handle_listen == NULL) {
				DWORD lpThreadID;
				monitor_run = monitor_begin;
				handle_listen = CreateThread(NULL, 0, ThreadListenProcess, NULL, 0, &lpThreadID);
			}
			Sleep(10);
		} while (monitor_run != monitor_ok);  // Wait for the listener thread to start
	}
	else {  // Disable monitoring
		handle_listen = NULL;
		monitor_run = monitor_exit;
	}

	if (err < 0) {
		return err_net_rx_timeout;
	}

	return NetRxReturnHandle(&rx, &tx);
}

/*
	Monitor the physical mouse left button status.
	Return values:
	-1: Monitoring not enabled, call kmNet_monitor(1) first.
	0 : Physical mouse left button released.
	1 : Physical mouse left button pressed.
*/
int kmNet_monitor_mouse_left()
{
	if (monitor_run != monitor_ok) return -1;  // Check if monitoring is active.
	return (hw_mouse.buttons & 0x01) ? 1 : 0;  // Check the left button status (bit 0).
}



/*
	Monitor the physical mouse middle button status.
	Return values:
	-1: Monitoring not enabled, call kmNet_monitor(1) first.
	0 : Physical mouse middle button released.
	1 : Physical mouse middle button pressed.
*/
int kmNet_monitor_mouse_middle()
{
	if (monitor_run != monitor_ok) return -1;  // Check if monitoring is active.
	return (hw_mouse.buttons & 0x04) ? 1 : 0;  // Check the middle button status (bit 2).
}


/*
	Monitor the physical mouse right button status.
	Return values:
	-1: Monitoring not enabled, call kmNet_monitor(1) first.
	0 : Physical mouse right button released.
	1 : Physical mouse right button pressed.
*/
int kmNet_monitor_mouse_right()
{
	if (monitor_run != monitor_ok) return -1;  // Check if monitoring is active.
	return (hw_mouse.buttons & 0x02) ? 1 : 0;  // Check the right button status (bit 1).
}

/*
	Monitor the physical mouse side button 1 status.
	Return values:
	-1: Monitoring not enabled, call kmNet_monitor(1) first.
	0 : Physical mouse side button 1 released.
	1 : Physical mouse side button 1 pressed.
*/
int kmNet_monitor_mouse_side1()
{
	if (monitor_run != monitor_ok) return -1;  // Check if monitoring is active.
	return (hw_mouse.buttons & 0x08) ? 1 : 0;  // Check the side button 1 status (bit 3).
}


/*
	Monitor the physical mouse side button 2 status.
	Return values:
	-1: Monitoring not enabled, call kmNet_monitor(1) first.
	0 : Physical mouse side button 2 released.
	1 : Physical mouse side button 2 pressed.
*/
int kmNet_monitor_mouse_side2()
{
	if (monitor_run != monitor_ok) return -1;  // Check if monitoring is active.
	return (hw_mouse.buttons & 0x10) ? 1 : 0;  // Check the side button 2 status (bit 4).
}

/*
	Monitor the status of a specific keyboard key.
	Return values:
	-1: Monitoring not enabled, call kmNet_monitor(1) first.
	0 : Keyboard key is not pressed.
	1 : Keyboard key is pressed.
*/
int kmNet_monitor_keyboard(short vkey)
{
	unsigned char vk_key = vkey & 0xff;  // Mask the input to get the key value.
	if (monitor_run != monitor_ok) return -1;  // Check if monitoring is active.

	if (vk_key >= KEY_LEFTCONTROL && vk_key <= KEY_RIGHT_GUI)  // Control keys
	{
		switch (vk_key)
		{
		case KEY_LEFTCONTROL: return (hw_keyboard.buttons & BIT0) ? 1 : 0;
		case KEY_LEFTSHIFT:   return (hw_keyboard.buttons & BIT1) ? 1 : 0;
		case KEY_LEFTALT:     return (hw_keyboard.buttons & BIT2) ? 1 : 0;
		case KEY_LEFT_GUI:    return (hw_keyboard.buttons & BIT3) ? 1 : 0;
		case KEY_RIGHTCONTROL:return (hw_keyboard.buttons & BIT4) ? 1 : 0;
		case KEY_RIGHTSHIFT:  return (hw_keyboard.buttons & BIT5) ? 1 : 0;
		case KEY_RIGHTALT:    return (hw_keyboard.buttons & BIT6) ? 1 : 0;
		case KEY_RIGHT_GUI:   return (hw_keyboard.buttons & BIT7) ? 1 : 0;
		}
	}
	else  // Regular keys
	{
		for (int i = 0; i < 10; i++)
		{
			if (hw_keyboard.data[i] == vk_key)
			{
				return 1;  // Key is pressed.
			}
		}
	}
	return 0;  // Key is not pressed.
}

/*
	Enable or disable internal box debug print and send the output to the specified port (for debugging purposes).
	port   : The port to which the debug information will be sent.
	enable : 1 to enable debug printing, 0 to disable.
	Return values:
	0: Success.
	Other values: Error codes.
*/
int kmNet_debug(short port, char enable)
{
	int err;
	if (sockClientfd <= 0) return err_creat_socket;  // Check if the socket is created.

	tx.head.indexpts++;              // Increment instruction count.
	tx.head.cmd = cmd_debug;         // Command for debug.
	tx.head.rand = port | (enable << 16);  // Set random obfuscation value (port + enable flag).

	int length = sizeof(cmd_head_t);  // Length of the command header.
	sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));  // Send the command.

	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);  // Wait for the response.

	if (err < 0) return err_net_rx_timeout;  // Handle timeout error.
	return NetRxReturnHandle(&rx, &tx);  // Process the response and return result.
}


/*
	Mask or unmask the mouse left button.
	enable : 1 to mask the mouse left button, 0 to unmask it.
	Return values:
	0: Success.
	Other values: Error codes.
*/
int kmNet_mask_mouse_left(int enable)
{
	int err;
	if (sockClientfd <= 0) return err_creat_socket;  // Check if the socket is created.

	tx.head.indexpts++;              // Increment instruction count.
	tx.head.cmd = cmd_mask_mouse;    // Command for masking the mouse.
	tx.head.rand = enable ? (mask_keyboard_mouse_flag |= BIT0) : (mask_keyboard_mouse_flag &= ~BIT0);  // Set the flag to mask/unmask the left button.

	int length = sizeof(cmd_head_t);  // Length of the command header.
	sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));  // Send the command.

	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);  // Wait for the response.

	if (err < 0) return err_net_rx_timeout;  // Handle timeout error.
	return NetRxReturnHandle(&rx, &tx);  // Process the response and return result.
}

/*
	Mask or unmask the mouse right button.
	enable : 1 to mask the mouse right button, 0 to unmask it.
	Return values:
	0: Success.
	Other values: Error codes.
*/
int kmNet_mask_mouse_right(int enable)
{
	int err;
	if (sockClientfd <= 0) return err_creat_socket;  // Check if the socket is created.

	tx.head.indexpts++;              // Increment instruction count.
	tx.head.cmd = cmd_mask_mouse;    // Command for masking the mouse.
	tx.head.rand = enable ? (mask_keyboard_mouse_flag |= BIT1) : (mask_keyboard_mouse_flag &= ~BIT1);  // Set the flag to mask/unmask the right button.

	int length = sizeof(cmd_head_t);  // Length of the command header.
	sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));  // Send the command.

	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);  // Wait for the response.

	if (err < 0) return err_net_rx_timeout;  // Handle timeout error.
	return NetRxReturnHandle(&rx, &tx);  // Process the response and return result.
}

/*
	Mask or unmask the mouse middle button.
	enable : 1 to mask the mouse middle button, 0 to unmask it.
	Return values:
	0: Success.
	Other values: Error codes.
*/
int kmNet_mask_mouse_middle(int enable)
{
	int err;
	if (sockClientfd <= 0) return err_creat_socket;  // Check if the socket is created.

	tx.head.indexpts++;              // Increment instruction count.
	tx.head.cmd = cmd_mask_mouse;    // Command for masking the mouse.
	tx.head.rand = enable ? (mask_keyboard_mouse_flag |= BIT2) : (mask_keyboard_mouse_flag &= ~BIT2);  // Set the flag to mask/unmask the middle button.

	int length = sizeof(cmd_head_t);  // Length of the command header.
	sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));  // Send the command.

	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);  // Wait for the response.

	if (err < 0) return err_net_rx_timeout;  // Handle timeout error.
	return NetRxReturnHandle(&rx, &tx);  // Process the response and return result.
}



/*
	Mask or unmask the mouse side button 1.
	enable : 1 to mask the mouse side button 1, 0 to unmask it.
	Return values:
	0: Success.
	Other values: Error codes.
*/
int kmNet_mask_mouse_side1(int enable)
{
	int err;
	if (sockClientfd <= 0) return err_creat_socket;  // Check if the socket is created.

	tx.head.indexpts++;              // Increment instruction count.
	tx.head.cmd = cmd_mask_mouse;    // Command for masking the mouse.
	tx.head.rand = enable ? (mask_keyboard_mouse_flag |= BIT3) : (mask_keyboard_mouse_flag &= ~BIT3);  // Set the flag to mask/unmask the side button 1.

	int length = sizeof(cmd_head_t);  // Length of the command header.
	sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));  // Send the command.

	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);  // Wait for the response.

	if (err < 0) return err_net_rx_timeout;  // Handle timeout error.
	return NetRxReturnHandle(&rx, &tx);  // Process the response and return result.
}



/*
	Mask or unmask the mouse side button 2.
	enable : 1 to mask the mouse side button 2, 0 to unmask it.
	Return values:
	0: Success.
	Other values: Error codes.
*/
int kmNet_mask_mouse_side2(int enable)
{
	int err;
	if (sockClientfd <= 0) return err_creat_socket;  // Check if the socket is created.

	tx.head.indexpts++;              // Increment instruction count.
	tx.head.cmd = cmd_mask_mouse;    // Command for masking the mouse.
	tx.head.rand = enable ? (mask_keyboard_mouse_flag |= BIT4) : (mask_keyboard_mouse_flag &= ~BIT4);  // Set the flag to mask/unmask the side button 2.

	int length = sizeof(cmd_head_t);  // Length of the command header.
	sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));  // Send the command.

	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);  // Wait for the response.

	if (err < 0) return err_net_rx_timeout;  // Handle timeout error.
	return NetRxReturnHandle(&rx, &tx);  // Process the response and return result.
}

/*
	Mask or unmask the mouse X-axis movement.
	enable : 1 to mask the mouse X-axis, 0 to unmask it.
	Return values:
	0: Success.
	Other values: Error codes.
*/
int kmNet_mask_mouse_x(int enable)
{
	int err;
	if (sockClientfd <= 0) return err_creat_socket;  // Check if the socket is created.

	tx.head.indexpts++;              // Increment instruction count.
	tx.head.cmd = cmd_mask_mouse;    // Command for masking the mouse.
	tx.head.rand = enable ? (mask_keyboard_mouse_flag |= BIT5) : (mask_keyboard_mouse_flag &= ~BIT5);  // Set the flag to mask/unmask the X-axis.

	int length = sizeof(cmd_head_t);  // Length of the command header.
	sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));  // Send the command.

	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);  // Wait for the response.

	if (err < 0) return err_net_rx_timeout;  // Handle timeout error.
	return NetRxReturnHandle(&rx, &tx);  // Process the response and return result.
}

/*
	Mask or unmask the mouse Y-axis movement.
	enable : 1 to mask the mouse Y-axis, 0 to unmask it.
	Return values:
	0: Success.
	Other values: Error codes.
*/
int kmNet_mask_mouse_y(int enable)
{
	int err;
	if (sockClientfd <= 0) return err_creat_socket;  // Check if the socket is created.

	tx.head.indexpts++;              // Increment instruction count.
	tx.head.cmd = cmd_mask_mouse;    // Command for masking the mouse.
	tx.head.rand = enable ? (mask_keyboard_mouse_flag |= BIT6) : (mask_keyboard_mouse_flag &= ~BIT6);  // Set the flag to mask/unmask the Y-axis.

	int length = sizeof(cmd_head_t);  // Length of the command header.
	sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));  // Send the command.

	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);  // Wait for the response.

	if (err < 0) return err_net_rx_timeout;  // Handle timeout error.
	return NetRxReturnHandle(&rx, &tx);  // Process the response and return result.
}

/*
	Mask or unmask the mouse wheel.
	enable : 1 to mask the mouse wheel, 0 to unmask it.
	Return values:
	0: Success.
	Other values: Error codes.
*/
int kmNet_mask_mouse_wheel(int enable)
{
	int err;
	if (sockClientfd <= 0) return err_creat_socket;  // Check if the socket is created.

	tx.head.indexpts++;              // Increment instruction count.
	tx.head.cmd = cmd_mask_mouse;    // Command for masking the mouse.
	tx.head.rand = enable ? (mask_keyboard_mouse_flag |= BIT7) : (mask_keyboard_mouse_flag &= ~BIT7);  // Set the flag to mask/unmask the wheel.

	int length = sizeof(cmd_head_t);  // Length of the command header.
	sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));  // Send the command.

	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);  // Wait for the response.

	if (err < 0) return err_net_rx_timeout;  // Handle timeout error.
	return NetRxReturnHandle(&rx, &tx);  // Process the response and return result.
}

/*
	Mask or unmask the specified keyboard key.
	vkey: The virtual key code of the key to mask/unmask.
	Return values:
	0: Success.
	Other values: Error codes.
*/
int kmNet_mask_keyboard(short vkey)
{
	int err;
	BYTE v_key = vkey & 0xff;  // Extract the lower byte of the virtual key.

	if (sockClientfd <= 0) return err_creat_socket;  // Check if the socket is created.

	tx.head.indexpts++;              // Increment instruction count.
	tx.head.cmd = cmd_mask_mouse;    // Command for masking the key.
	tx.head.rand = (mask_keyboard_mouse_flag & 0xff) | (v_key << 8);  // Set the random flag with the virtual key.

	int length = sizeof(cmd_head_t);  // Length of the command header.
	sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));  // Send the command.

	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);  // Wait for the response.

	if (err < 0) return err_net_rx_timeout;  // Handle timeout error.
	return NetRxReturnHandle(&rx, &tx);  // Process the response and return result.
}

/*
	Unmask the specified keyboard key.
	vkey: The virtual key code of the key to unmask.
	Return values:
	0: Success.
	Other values: Error codes.
*/
int kmNet_unmask_keyboard(short vkey)
{
	int err;
	BYTE v_key = vkey & 0xff;  // Extract the lower byte of the virtual key.

	if (sockClientfd <= 0) return err_creat_socket;  // Check if the socket is created.

	tx.head.indexpts++;               // Increment instruction count.
	tx.head.cmd = cmd_unmask_all;      // Command for unmasking the key.
	tx.head.rand = (mask_keyboard_mouse_flag & 0xff) | (v_key << 8);  // Set the random flag with the virtual key.

	int length = sizeof(cmd_head_t);  // Length of the command header.
	sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));  // Send the command.

	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);  // Wait for the response.

	if (err < 0) return err_net_rx_timeout;  // Handle timeout error.
	return NetRxReturnHandle(&rx, &tx);  // Process the response and return result.
}

/*
	Unmask all physical key and mouse masks that have been previously set.
	Return values:
	0: Success.
	Other values: Error codes.
*/
int kmNet_unmask_all()
{
	int err;

	if (sockClientfd <= 0) return err_creat_socket;  // Check if the socket is created.

	tx.head.indexpts++;               // Increment instruction count.
	tx.head.cmd = cmd_unmask_all;      // Command for unmasking all keys and mouse buttons.

	mask_keyboard_mouse_flag = 0;      // Reset the mask flag (unmask all).

	tx.head.rand = mask_keyboard_mouse_flag;  // Set the random flag with the updated mask.

	int length = sizeof(cmd_head_t);  // Length of the command header.
	sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));  // Send the command.

	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);  // Wait for the response.

	if (err < 0) return err_net_rx_timeout;  // Handle timeout error.
	return NetRxReturnHandle(&rx, &tx);  // Process the response and return result.
}

/*
	Set the configuration with the specified IP address and port.
	This function sends the IP and port to the server to update the configuration.
	Arguments:
	- ip: A string representing the IP address.
	- port: The port number.
	Return values:
	0: Success.
	Other values: Error codes.
*/
int kmNet_setconfig(char* ip, unsigned short port)
{
	int err;

	if (sockClientfd <= 0) return err_creat_socket;  // Check if the socket is created.

	tx.head.indexpts++;                 // Increment instruction count.
	tx.head.cmd = cmd_setconfig;         // Command for setting configuration.
	tx.head.rand = inet_addr(ip);        // Set the IP address in the command structure.

	tx.u8buff.buff[0] = port >> 8;       // Extract the high byte of the port number.
	tx.u8buff.buff[1] = port & 0xFF;     // Extract the low byte of the port number.

	int length = sizeof(cmd_head_t) + 2;  // Total length of the command (header + 2 bytes for port).
	sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));  // Send the command.

	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);  // Wait for the response.

	if (err < 0) return err_net_rx_timeout;  // Handle timeout error.
	return NetRxReturnHandle(&rx, &tx);  // Process the response and return result.
}

/*
	Set the VID and PID for the device at the box end.
	This function sends the VID and PID to the server to configure the device.
	Arguments:
	- vid: The vendor ID.
	- pid: The product ID.
	Return values:
	0: Success.
	Other values: Error codes.
*/
int kmNet_setvidpid(unsigned short vid, unsigned short pid)
{
	int err;

	if (sockClientfd <= 0) return err_creat_socket;  // Check if the socket is created.

	tx.head.indexpts++;                 // Increment instruction count.
	tx.head.cmd = cmd_setvidpid;         // Command for setting VID and PID.
	tx.head.rand = vid | (pid << 16);    // Set the VID and PID in the random field (VID in lower 16 bits, PID in upper 16 bits).

	int length = sizeof(cmd_head_t);     // Length of the command (only header in this case).
	sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));  // Send the command.

	SOCKADDR_IN sclient;
	int clen = sizeof(sclient);
	err = recvfrom(sockClientfd, (char*)&rx, 1024, 0, (struct sockaddr*)&sclient, &clen);  // Wait for the response.

	if (err < 0) return err_net_rx_timeout;  // Handle timeout error.
	return NetRxReturnHandle(&rx, &tx);  // Process the response and return result.
}

/*
	Fill the entire LCD screen with the specified color.
	You can use black (0x0000) for clearing the screen.
	Arguments:
	- rgb565: The color in RGB565 format (5 bits for red, 6 bits for green, 5 bits for blue).
	Return values:
	0: Success.
	Other values: Error codes.
*/
int kmNet_lcd_color(unsigned short rgb565)
{
	int err;

	if (sockClientfd <= 0) return err_creat_socket;  // Check if the socket is created.

	// Iterate through the 40 rows of the LCD screen.
	for (int y = 0; y < 40; y++)
	{
		tx.head.indexpts++;            // Increment instruction count.
		tx.head.cmd = cmd_showpic;     // Command to show picture.
		tx.head.rand = 0 | y * 4;      // Set the random field to the row index.

		// Fill the buffer with the RGB565 color for the entire row.
		for (int c = 0; c < 512; c++)
			tx.u16buff.buff[c] = rgb565;

		int length = sizeof(cmd_head_t) + 1024;  // Length of the command (header + 1024 bytes of pixel data).
		sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));  // Send the command.

		SOCKADDR_IN sclient;
		int clen = sizeof(sclient);
		err = recvfrom(sockClientfd, (char*)&rx, length, 0, (struct sockaddr*)&sclient, &clen);  // Wait for the response.

		if (err < 0) return err_net_rx_timeout;  // Handle timeout error.
	}

	return NetRxReturnHandle(&rx, &tx);  // Process the response and return result.
}

/*
    Display a 128x80 image at the bottom of the screen.
    Arguments:
    - buff_128_80: A pointer to the image buffer (128x80 pixels).
    Return values:
    0: Success.
    Other values: Error codes.
*/
int kmNet_lcd_picture_bottom(unsigned char* buff_128_80)
{
    int err;

    if (sockClientfd <= 0) return err_creat_socket;  // Check if the socket is created.

    // Iterate through the 20 rows of the image.
    for (int y = 0; y < 20; y++)
    {
        tx.head.indexpts++;            // Increment instruction count.
        tx.head.cmd = cmd_showpic;     // Command to show picture.
        tx.head.rand = 80 + y * 4;     // Set the random field to the row index.

        // Copy the image data for the current row into the buffer.
        memcpy(tx.u8buff.buff, &buff_128_80[y * 1024], 1024);

        int length = sizeof(cmd_head_t) + 1024;  // Length of the command (header + 1024 bytes of image data).
        sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));  // Send the command.

        SOCKADDR_IN sclient;
        int clen = sizeof(sclient);
        err = recvfrom(sockClientfd, (char*)&rx, length, 0, (struct sockaddr*)&sclient, &clen);  // Wait for the response.

        if (err < 0) return err_net_rx_timeout;  // Handle timeout error.
    }

    return NetRxReturnHandle(&rx, &tx);  // Process the response and return result.
}


/*
	Display a 128x160 image on the screen.
	Arguments:
	- buff_128_160: A pointer to the image buffer (128x160 pixels).
	Return values:
	0: Success.
	Other values: Error codes.
*/
int kmNet_lcd_picture(unsigned char* buff_128_160)
{
	int err;

	if (sockClientfd <= 0) return err_creat_socket;  // Check if the socket is created.

	// Iterate through the 40 rows of the image.
	for (int y = 0; y < 40; y++)
	{
		tx.head.indexpts++;            // Increment instruction count.
		tx.head.cmd = cmd_showpic;     // Command to show picture.
		tx.head.rand = y * 4;          // Set the random field to the row index.

		// Copy the image data for the current row into the buffer.
		memcpy(tx.u8buff.buff, &buff_128_160[y * 1024], 1024);

		int length = sizeof(cmd_head_t) + 1024;  // Length of the command (header + 1024 bytes of image data).
		sendto(sockClientfd, (const char*)&tx, length, 0, (struct sockaddr*)&addrSrv, sizeof(addrSrv));  // Send the command.

		SOCKADDR_IN sclient;
		int clen = sizeof(sclient);
		err = recvfrom(sockClientfd, (char*)&rx, length, 0, (struct sockaddr*)&sclient, &clen);  // Wait for the response.

		if (err < 0) return err_net_rx_timeout;  // Handle timeout error.
	}

	return NetRxReturnHandle(&rx, &tx);  // Process the response and return result.
}
