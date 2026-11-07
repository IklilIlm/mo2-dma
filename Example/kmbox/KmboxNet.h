#pragma once
#include <Winsock2.h>
#include <stdio.h>
#include <time.h>
#include <iostream>
#include <iomanip>
#include <WS2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#pragma warning(disable : 4996)

// Command codes
#define 	cmd_connect			0xaf3c2828 // OK: Connect to the box
#define     cmd_mouse_move		0xaede7345 // OK: Mouse move control
#define		cmd_mouse_left		0x9823AE8D // OK: Left mouse button control
#define		cmd_mouse_middle	0x97a3AE8D // OK: Middle mouse button control
#define		cmd_mouse_right		0x238d8212 // OK: Right mouse button control
#define		cmd_mouse_wheel		0xffeead38 // OK: Mouse wheel control
#define     cmd_mouse_automove	0xaede7346 // OK: Mouse auto-move (simulate human movement)
#define     cmd_keyboard_all    0x123c2c2f // OK: Keyboard all parameters control
#define		cmd_reboot			0xaa8855aa // OK: Reboot the box
#define     cmd_bazerMove       0xa238455a // OK: Mouse Bezier movement
#define     cmd_monitor         0x27388020 // OK: Monitor physical keyboard and mouse data on the box
#define     cmd_debug           0x27382021 // OK: Enable debug information
#define     cmd_mask_mouse      0x23234343 // OK: Mask physical keyboard and mouse
#define     cmd_unmask_all      0x23344343 // OK: Unmask all physical keyboard and mouse
#define     cmd_setconfig       0x1d3d3323 // OK: Set IP configuration
#define     cmd_setvidpid       0xffed3232 // OK: Set device VID/PID
#define     cmd_showpic         0x12334883 // OK: Display picture



extern SOCKET sockClientfd; // Socket communication handle

// Command header structure
typedef struct
{
	unsigned int mac;        // Box's MAC address (mandatory)
	unsigned int rand;       // Random value
	unsigned int indexpts;   // Timestamp
	unsigned int cmd;        // Command code
} cmd_head_t;


typedef struct
{
	unsigned char buff[1024];	//
}cmd_data_t;
typedef struct
{
	unsigned short buff[512];	//
}cmd_u16_t;

// Mouse data structure
typedef struct
{
	int button;     // Button state (e.g., left, right, middle button)
	int x;          // X-coordinate of the mouse position
	int y;          // Y-coordinate of the mouse position
	int wheel;      // Scroll wheel state (e.g., scrolling movement)
	int point[10];  // Reserved for Bezier curve control (store up to 5th-order derivatives)
} soft_mouse_t;


// Keyboard data structure
typedef struct
{
	char ctrl;        // Control key state (e.g., indicating if Ctrl key is pressed or not)
	char resvel;      // Reserved field, could be used for other purposes (e.g., velocity, additional flags, etc.)
	char button[10];  // Array of 10 characters to represent the state of individual keys (e.g., 0 for not pressed, 1 for pressed)
} soft_keyboard_t;

// Union for different command data types
typedef struct
{
	cmd_head_t head;         // Header for the command (contains basic information like MAC address, command, etc.)
	union {
		cmd_data_t      u8buff;        // Buffer for 8-bit data
		cmd_u16_t       u16buff;       // Buffer for 16-bit data
		soft_mouse_t    cmd_mouse;     // Mouse command data
		soft_keyboard_t cmd_keyboard; // Keyboard command data
	};
} client_tx;

enum
{
	err_creat_socket = -9000,      // Failed to create socket
	err_net_version,               // Socket version mismatch or error
	err_net_tx,                    // Error during socket transmission (send)
	err_net_rx_timeout,            // Timeout error during socket reception (receive)
	err_net_cmd,                   // Invalid or unrecognized command error
	err_net_pts,                   // Timestamp mismatch error
	success = 0,                   // Operation completed successfully
	usb_dev_tx_timeout,            // USB device transmission timeout error
};



/*
The input parameters for connecting to the kmboxNet box are as follows:
ip  : The IP address of the box (it will be displayed on the screen)
port: The communication port number (it will be displayed on the screen)
mac : The MAC address of the box (it will be displayed on the screen)
Return value: Returns 0 if the connection is successful, other values refer to error codes
*/
int kmNet_init(char* ip, char* port, char* mac);//ok

int kmNet_mouse_move(short x, short y);			//ok
int kmNet_mouse_left(int isdown);				//ok
int kmNet_mouse_right(int isdown);				//ok
int kmNet_mouse_middle(int isdown);				//ok
int kmNet_mouse_wheel(int wheel);				//ok
int kmNet_mouse_all(int button, int x, int y, int wheel);//ok
int kmNet_mouse_move_auto(int x, int y, int time_ms);	//ok
int kmNet_mouse_move_beizer(int x, int y, int ms, int x1, int y1, int x2, int y2);//Second-order curve


// Keyboard functions
int kmNet_keydown(int vkey); // ok
int kmNet_keyup(int vkey);   // ok


// Monitoring series
int kmNet_monitor(int enable);            // Enable or disable physical keyboard and mouse monitoring
int kmNet_monitor_mouse_left();           // Query the physical mouse left button status
int kmNet_monitor_mouse_middle();         // Query the mouse middle button status
int kmNet_monitor_mouse_right();          // Query the mouse right button status
int kmNet_monitor_mouse_side1();          // Query the mouse side button 1 status
int kmNet_monitor_mouse_side2();          // Query the mouse side button 2 status
int kmNet_monitor_keyboard(short vk_key); // Query the status of a specified keyboard key

// Physical keyboard and mouse masking series
int kmNet_mask_mouse_left(int enable);    // Mask the left mouse button
int kmNet_mask_mouse_right(int enable);   // Mask the right mouse button
int kmNet_mask_mouse_middle(int enable);  // Mask the middle mouse button
int kmNet_mask_mouse_side1(int enable);   // Mask the mouse side button 1
int kmNet_mask_mouse_side2(int enable);   // Mask the mouse side button 2
int kmNet_mask_mouse_x(int enable);       // Mask the mouse X-axis coordinate
int kmNet_mask_mouse_y(int enable);       // Mask the mouse Y-axis coordinate
int kmNet_mask_mouse_wheel(int enable);   // Mask the mouse wheel
int kmNet_mask_keyboard(short vkey);      // Mask the specified keyboard key
int kmNet_unmask_keyboard(short vkey);    // Unmask the specified keyboard key
int kmNet_unmask_all();                   // Unmask all previously set physical masks



// Configuration functions
int kmNet_reboot(void);                                          // Reboot the box
int kmNet_setconfig(char* ip, unsigned short port);              // Configure the box's IP address
int kmNet_setvidpid(unsigned short vid, unsigned short pid);     // Set the box's VID/PID. The box needs to be rebooted for it to take effect after power on
int kmNet_debug(short port, char enable);                         // Enable or disable debugging
int kmNet_lcd_color(unsigned short rgb565);                       // Fill the entire LCD screen with the specified color. Black can be used for clearing
int kmNet_lcd_picture_bottom(unsigned char* buff_128_80);        // Display a 128x80 image at the bottom of the screen
int kmNet_lcd_picture(unsigned char* buff_128_160);               // Display a 128x160 image on the entire screen



// Do not call the following functions (Hardware descriptors used to update all parameters on the device side)
int kmNet_DevDescriptor(int wrflag, char* buff, int length);          // Read or write the device descriptor
int kmNet_CfgDescriptor(int wrflag, char* buff, int length);          // Read or write the device configuration descriptor
int kmNet_EndPointDescriptor(int wrflag, int ep, char* buff, int length); // Read or write the endpoint descriptor


// Video capture class functions
#if 0
int kvm_setvideo(int width, int height, int fps);  // Set the video capture resolution and frame rate
int kvm_getframe(Mat* frame);                       // Get the latest frame image data
int kvm_load_yolo_module(char* path);               // Load the YOLO model from the specified path
int kvm_run_yolo(Mat* frame, int cpu_gpu);          // Perform image inference using YOLO
#endif

