// Joyconでパワポを操作するやつ
// 機能の追加は未定

#include <windows.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <time.h>
#include <setupapi.h>
#include "hidsdi.h"
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")

#define DLLIMPORT __declspec(dllimport) __stdcall
extern "C" HANDLE DLLIMPORT OpenHidHandle(unsigned short vendor_id, unsigned short product_id);
extern "C" void DLLIMPORT ReadReport(HANDLE handle, unsigned char* buffer, int *read_bytes_length);
extern "C" void DLLIMPORT WriteReport(HANDLE handle, unsigned char* buffer, int *written_bytes_length);
extern "C" void DLLIMPORT CloseHidHandle(HANDLE handle);


void SetOutput_0x01();
void SetReportSubcmd(unsigned char subcmd, unsigned char data);
void Wait(int milli_sec);
void PutsRed(char *string);
void JoyconInput();
void JoyconInputLoop();
void StartJoyconThread();
void StopJoyconThread();
int JoyconThreadCheck();
void ParseInputData();
void EmulateKeyboard();
void ShowBatteryStatus();

// subcommands
#define REQUEST_DEVICE_INFO 0x02
#define SET_INPUT_REPORT_MODE 0x03
#define LED_CONTROL 0x30
#define SET_IMU_STATUS 0x40

// input_report_mode options
#define STD_FULL_MODE 0x30
#define HID_GAMEPAD_MODE 0x3F

// led pattern
#define LED1_ON 0x01
#define LED2_ON 0x02
#define LED3_ON 0x04
#define LED4_ON 0x08
#define LED1_BLINK 0x10
#define LED2_BLINK 0x20
#define LED3_BLINK 0x40
#define LED4_BLINK 0x80
#define LED_OFF 0x00

// IMU state
#define IMU_ENABLE 0x01
#define IMU_DISABLE 0x00


HANDLE joycon_handle;
const USHORT VENDOR_ID = 0x057e; // Nintendo
const USHORT PRODUCT_ID_R = 0x2007; // JonCon - R
const USHORT PRODUCT_ID_L = 0x2006; // JoyCon - L
USHORT selected_pid; // L, RのどちらかのPIDが入る

unsigned char thread_loop_flag;
unsigned char thread_alive;
uintptr_t thread_handle;

// 生データ
unsigned char	res_type;
unsigned char	button1,button2;
unsigned int	StickX,StickY;
unsigned int	Ax,Ay,Az;
unsigned int	Rx,Ry,Rz;


// 成形済みデータ
typedef struct parsed_button_data_ {
unsigned char up, down, left, right;
unsigned char a, b, x, y;
unsigned char plus, minus, home, capture;
unsigned char l, r, zl, zr, sl, sr;
} parsed_button_data;

parsed_button_data button;


#define MAXREPORTSIZE 256
int	input_length, output_length;
UCHAR input_report[MAXREPORTSIZE];
UCHAR output_report[MAXREPORTSIZE];
ULONG packet_num = 0;



void SetOutput_0x01(void)
{
	output_report[0] = 0x01;
    output_report[1] = (++packet_num) % 16; // 0 ~ F で
    for (int i = 2; i < 10; i++){ 
       output_report[i] = 0;
    }
}


void SetReportSubcmd(unsigned char subcmd, unsigned char data)
{
    SetOutput_0x01();
    output_report[10] = subcmd;
    output_report[11] = data;
    WriteReport(joycon_handle, output_report, &output_length);
    ReadReport(joycon_handle, input_report, &input_length);
}


void Wait(int milli_sec)
{
    LARGE_INTEGER f;
	LARGE_INTEGER t1,t2;
	double t;
	QueryPerformanceFrequency(&f);
	QueryPerformanceCounter(&t1);
	while (1) {
		QueryPerformanceCounter(&t2);
		t = (double)t2.QuadPart - (double)t1.QuadPart;
		t /= (double)f.QuadPart;
		t *= 1000;
		if (t>(double)milli_sec) { return; }
	}
}


void PutsRed(char *str)
{
    printf("\x1b[31m"); // 赤色にする
	puts(str);
    printf("\x1b[39m"); // 元の色に戻す
}


void JoyconInput(void)
{
    int l;
    ReadReport(joycon_handle, input_report, &input_length);
    res_type = input_report[0];
    if (input_length == 362) {
        if (selected_pid == PRODUCT_ID_L)
        {
            button1 = input_report[5]; // 十字キー       
            StickX = input_report[6] | ((unsigned int)input_report[7] & 0x0F) << 8;
            StickY = (input_report[7]>>4 ) | (input_report[8] <<4 );
        }
        else
        {
            button1 = input_report[3]; // ABXY
            StickX = input_report[9] | ((unsigned int)input_report[10]&0x0F)<<8;
		    StickY = (input_report[10]>>4 ) | (input_report[11] <<4 );
        }
        button2 = input_report[4]; // それ以外
        Ax=input_report[13] | (unsigned int)input_report[14]<<8;
		Ay=input_report[15] | (unsigned int)input_report[16]<<8;
		Az=input_report[17] | (unsigned int)input_report[18]<<8;
		Rx=input_report[19] | (unsigned int)input_report[20]<<8;
		Ry=input_report[21] | (unsigned int)input_report[22]<<8;
		Rz=input_report[23] | (unsigned int)input_report[24]<<8;
    }
    else
    {
        printf("\x1b[31m");   /* 文字色を赤に */
		printf("type : %d, length : %d\r", res_type, input_length);
        printf("\x1b[39m");
	}
}


void JoyconInputLoop(void *)
{
    while(thread_loop_flag)
    {
        thread_alive = 1;
        JoyconInput();
        Wait(16);
    }
    thread_alive = 0;
    return;
}


void StartJoyconThread()
{
    thread_loop_flag = 1;
    thread_handle = _beginthread(JoyconInputLoop, 0, NULL);
}


void StopJoyconThread()
{
    thread_loop_flag = 0;
    Wait(100);
    if (thread_alive == 1) // 何らかの理由でスレッドが生きている時
    {
        TerminateThread((HANDLE)thread_handle, -1);
    }
}


// Joyconと通信しているスレッド動いているか確認する
int JoyconThreadCheck()
{
    res_type = 0;
    Wait(100);
    return res_type != 0;
}


// ボタンの押した判定に使う
// 押してあったら++, 押してなかったら0にする
void ParseInputData()
{
    if (selected_pid == PRODUCT_ID_L)
    {
        button.down = (input_report[5] & 0x01) ? button.down + 1 : 0;
        button.up = (input_report[5] & 0x02) ? button.up + 1 : 0; 
        button.right = (input_report[5] & 0x04) ? button.right + 1 : 0; 
        button.left = (input_report[5] & 0x08) ? button.left + 1 : 0;
        button.sr = (input_report[5] & 0x10) ? button.sr + 1 : 0;
        button.sl = (input_report[5] & 0x20) ? button.sl + 1 : 0;
        button.l = (input_report[5] & 0x40) ? button.l + 1 : 0;
        button.zl = (input_report[5] & 0x80) ? button.zl + 1 : 0;
    }
    else 
    {
        button.y = (input_report[3] & 0x01) ? button.y + 1 : 0;
        button.x = (input_report[3] & 0x02) ? button.x + 1 : 0; 
        button.b = (input_report[3] & 0x04) ? button.b + 1 : 0; 
        button.a = (input_report[3] & 0x08) ? button.a + 1 : 0;
        button.sr = (input_report[3] & 0x10) ? button.sr + 1 : 0;
        button.sl = (input_report[3] & 0x20) ? button.sl + 1 : 0;
        button.r = (input_report[3] & 0x40) ? button.r + 1 : 0;
        button.zr = (input_report[3] & 0x80) ? button.zr + 1 : 0;
    }
    button.minus = (input_report[4] & 0x01) ? button.minus + 1 : 0;
    button.plus = (input_report[4] & 0x02) ? button.plus + 1 : 0; 
    button.home = (input_report[3] & 0x10) ? button.home + 1 : 0;
    button.capture = (input_report[3] & 0x20) ? button.capture + 1 : 0;
}


// キーの入力をエミュレートする
void EmulateKeyboard()
{
    if (button.up == 1)
    {
        keybd_event(VK_UP, 0, 0, 0);
        keybd_event(VK_UP, 0, KEYEVENTF_KEYUP, 0);
    }
    if (button.down == 1)
    {
        keybd_event(VK_DOWN, 0, 0, 0);
        keybd_event(VK_DOWN, 0, KEYEVENTF_KEYUP, 0);
    }
    if (button.left == 1)
    {
        keybd_event(VK_LEFT, 0, 0, 0);
        keybd_event(VK_LEFT, 0, KEYEVENTF_KEYUP, 0);
    }
    if (button.right == 1)
    {
        keybd_event(VK_RIGHT, 0, 0, 0);
        keybd_event(VK_RIGHT, 0, KEYEVENTF_KEYUP, 0);
    }

    if (button.sr > 0 && button.sl > 0 && button.minus > 0)
    {
        keybd_event(VK_ESCAPE, 0, 0, 0);
        keybd_event(VK_ESCAPE, 0, KEYEVENTF_KEYUP, 0);
    }
}


// LEDでJoyconの電池残量を確認する
void ShowBatteryStatus()
{
    unsigned char battery = (input_report[2] & 0xF0) >> 4;
    switch(battery)
    {
        case 8:
            battery = LED1_ON | LED2_ON | LED3_ON | LED4_ON;
            break;
        case 6:
            battery = LED3_ON | LED4_ON;
            break;
        case 4:
            battery = LED4_ON;
            break;
        case 2:
            battery = LED4_BLINK;
            break;
        case 1:
            battery = LED1_BLINK | LED2_BLINK | LED3_BLINK | LED4_BLINK;
            break;
        default:
            battery = LED_OFF;
            break;
    }
    SetReportSubcmd(LED_CONTROL, battery);
}


int main()
{
    printf("JoyCon Presentation Remote\n");
    selected_pid = PRODUCT_ID_L;
    // selected_pid = PRODUCT_ID_R 
    joycon_handle = OpenHidHandle(VENDOR_ID, selected_pid);
    if (joycon_handle == INVALID_HANDLE_VALUE)
    {
        PutsRed("JoyCon Not Found");
        return 1;
    }

    SetReportSubcmd(SET_INPUT_REPORT_MODE, HID_GAMEPAD_MODE);
    SetReportSubcmd(LED_CONTROL, LED1_ON);
    SetReportSubcmd(SET_INPUT_REPORT_MODE, STD_FULL_MODE);
    SetReportSubcmd(SET_IMU_STATUS, IMU_ENABLE);
    SetReportSubcmd(LED_CONTROL, LED1_BLINK);
    StartJoyconThread();

    if (JoyconThreadCheck())
    {
        ShowBatteryStatus();
        while (1) 
        {
            if (button.sr > 0 && button.sl > 0 && button.l > 0 && button.zl > 0) {
                break;
            }
            ParseInputData();
            EmulateKeyboard();
            printf("stickX= %u , stickY= %u , Acc=%04X,%04X,%04X , Gyr=%04X,%04X,%04X\r", 
            StickX, StickY,
            Ax,Ay,Az,Rx,Ry,Rz);
            Wait(10);
        }
    }
    else
    {
        PutsRed("JoyCon Stop");  
    }
    StopJoyconThread();
    SetReportSubcmd(LED_CONTROL, LED_OFF);
    SetReportSubcmd(SET_IMU_STATUS, IMU_DISABLE);
    SetReportSubcmd(SET_INPUT_REPORT_MODE, HID_GAMEPAD_MODE);
    CloseHidHandle(joycon_handle);
    return 0;
}