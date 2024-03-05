#include <iostream>
#include <windows.h>
#include <opencv2/opencv.hpp>
#include <baseapi.h>
#include <allheaders.h>
#include <filesystem>
#include <fstream>
#include <ctime>
#include <codecvt>
#include "json.hpp"
#include "difflib.h"

typedef struct vkeys
{
	WORD vKey;
	char c;
} vkeys_t;

using namespace std;
using namespace cv;
using namespace tesseract;
using namespace difflib;
using json = nlohmann::json;
namespace fs = std::filesystem;

const char* WND_NAME_1 = "BlueStacks";
const char* WND_NAME_2 = "BlueStacks App Player";
const int MIN_WIDTH = 451, MIN_HEIGHT = 775;
const int LEN_VKEYS = 36;
const vkeys_t VKEYS[LEN_VKEYS] =
{
	{0x30, '0'},
	{0x31, '1'},
	{0x32, '2'},
	{0x33, '3'},
	{0x34, '4'},
	{0x35, '5'},
	{0x36, '6'},
	{0x37, '7'},
	{0x38, '8'},
	{0x39, '9'},
	{0x41, 'a'},
	{0x42, 'b'},
	{0x43, 'c'},
	{0x44, 'd'},
	{0x45, 'e'},
	{0x46, 'f'},
	{0x47, 'g'},
	{0x48, 'h'},
	{0x49, 'i'},
	{0x4A, 'j'},
	{0x4B, 'k'},
	{0x4C, 'l'},
	{0x4D, 'm'},
	{0x4E, 'n'},
	{0x4F, 'o'},
	{0x50, 'p'},
	{0x51, 'q'},
	{0x52, 'r'},
	{0x53, 's'},
	{0x54, 't'},
	{0x55, 'u'},
	{0x56, 'v'},
	{0x57, 'w'},
	{0x58, 'x'},
	{0x59, 'y'},
	{0x5A, 'z'}
};
const int LEN_LANGS = 2;
const string LANGUAGES [LEN_LANGS] = {"ita", "eng"};
const short int MAX_LENGTH = 256;
const int THRESH = 35;

const int _SIZE = 1;
const Mat ELEMENT = getStructuringElement(MORPH_RECT, Size(2 * _SIZE + 1, 2 * _SIZE + 1), Point(_SIZE, _SIZE));
const Scalar UP_TEXT = Scalar(255, 10, 226);
const Scalar DN_TEXT = Scalar(0, 0, 208);
const Scalar UP_GRN = Scalar(71, 255, 249);
const Scalar DN_GRN = Scalar(66, 210, 190);
const Scalar UP_PPL = Scalar(147, 167, 253);
const Scalar DN_PPL = Scalar(137, 130, 217);
const HWND hWnd_console = GetConsoleWindow();
HWND hWnd_found;

BOOL CALLBACK EnumWindowsProc(HWND, LPARAM);
void keyPress(WORD);
void keyRelease(WORD);
WORD getCorrKey(char);
Mat capture(HWND, int, int, RECT);
Rect biggestRect(vector<vector<Point>>);
int inLanguages(string);
string strToLower(string);
string textFromImage(Mat);
bool isBox(vector<vector<Point>>, int);
void writeAnswers(json, Rect, Rect, RECT, int, int, int, int);
void mouseClick(int, int, int, int);
void pressKeys(string);

int main()
{
	WINDOWPLACEMENT placement;
	BOOL minimized;
	RECT rect, rect_c;
	Mat image, image_proc, image_hsv, test;
	vector<vector<Point>> contours;
	Rect r_text, r_confirm, r_bonus;
	WORD kValue;
	int width, height, width_c, height_c, scrw, scrh, pos_x, pos_y;
	int i;
	string okay;
	string lang, filename, filename_current, filename_chosen, tessdata_path, rec_text, str_proc;
	double ratio_, best;
	bool found;
	wchar_t path[256];
	wstring ws;
	string* entries;
	ifstream in;
	json file, settings_file;
	TessBaseAPI* api = new TessBaseAPI();

	//settings parameters
	int ms, thr;

	scrw = GetSystemMetrics(SM_CXSCREEN);
	scrh = GetSystemMetrics(SM_CYSCREEN);

	GetModuleFileName(NULL, path, 256);
	ws = wstring(path);
	filename = string(ws.begin(), ws.end());
	filename = filename.substr(0, filename.find_last_of('\\') + 1);
	tessdata_path = filename;

	cout << "Loading settings from file \"settings.json\"..." << endl;
	
	in.open("settings.json");
	if (in.fail())
	{
		cout << "File \"settings.json\" was not found! Proceeding with default settings..." << endl;
		ms = 35;
		thr = 15;
	}
	else {
		settings_file = json::parse(in);
		in.close();

		ms = settings_file["sleep_time"];
		thr = settings_file["buy_time_every"];
	}

	lang = new char[3];
	while (true)
	{
		cout << "Insert language [ita - eng]: ";
		cin >> lang;
		while (!inLanguages(strToLower(lang)))
		{
			cin.clear();
			cin.ignore(246, '\n');
			cout << "Insert language [ita - eng]: ";
			cin >> lang;
		}
		
		EnumWindows(EnumWindowsProc, 0);
		
		placement.length = sizeof(placement);
		if (GetWindowPlacement(hWnd_found, &placement)) {
			minimized = (placement.showCmd & SW_SHOWMINIMIZED) != 0;
			if (minimized) {
				cout << "Window is minimized, unminimizing..." << endl;
				placement.showCmd = SW_SHOWNORMAL;
				SetWindowPlacement(hWnd_found, &placement);
			}
		}

		GetWindowRect(hWnd_found, &rect);
		width = rect.right - rect.left;
		height = rect.bottom - rect.top;

		GetWindowRect(hWnd_console, &rect_c);
		width_c = rect_c.right - rect_c.left;
		height_c = rect_c.bottom - rect_c.top;

		if (!hWnd_found) {
			cout << "No window named \"Bluestacks\"has been found" << endl;
			return 0;
		}

		pos_x = rect.left;
		pos_y = rect.top;
		if (height < MIN_HEIGHT)
			height = MIN_HEIGHT;
		if (width < MIN_WIDTH)
			width = MIN_WIDTH;
		if (rect.right > scrw)
			pos_x = rect.left - (rect.right - scrw);
		if (rect.bottom > scrh)
			pos_y = rect.top - (rect.bottom - scrh) - 40;
		if (rect.left < 0)
			pos_x = 0;
		if (rect.top < 0)
			pos_y = 0;

		SetWindowPos(hWnd_found, HWND_TOPMOST, pos_x, pos_y, width, height, SWP_SHOWWINDOW);
		SetWindowPos(hWnd_found, HWND_NOTOPMOST, pos_x, pos_y, width, height, SWP_SHOWWINDOW);
		SetWindowPos(hWnd_found, HWND_NOTOPMOST, pos_x, pos_y, width, height, SWP_NOSIZE);

		image = capture(hWnd_found, width, height, rect);
		cvtColor(image, image_hsv, COLOR_BGR2HSV);

		inRange(image_hsv, DN_TEXT, UP_TEXT, image_proc);
		erode(image_proc, image_proc, ELEMENT);
		dilate(image_proc, image_proc, ELEMENT);

		findContours(image_proc, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
		if (!contours.empty() && isBox(contours, 600))
		{
			r_text = biggestRect(contours);
			image_proc = image(Range(r_text.y, r_text.height + r_text.y), Range(r_text.x, r_text.width + r_text.x));
			cvtColor(image_proc, image_proc, COLOR_BGR2GRAY);
			threshold(image_proc, image_proc, 128, 255, THRESH_BINARY | THRESH_OTSU);

			cout << filename << endl;
			if (api->Init(filename.c_str(), lang.c_str()))
			{
				cout << "Could not initialize tesseract. Exiting..." << endl;
				return 1;
			}

			api->SetImage((uchar*)image_proc.data, image_proc.size().width, image_proc.size().height, image_proc.channels(), image_proc.step1());
			api->Recognize(0);
			rec_text = api->GetUTF8Text();

			api->End();
			rec_text = rec_text.substr(0, rec_text.find_last_of("\n")) + ".json";

			filename_current = filename + "answers_" + lang + "\\";

			found = false;
			filename_chosen = "";
			best = 0;
			for (const fs::directory_entry &entry : fs::directory_iterator(filename_current)) {
				str_proc = entry.path().string();
				str_proc = str_proc.substr(str_proc.find_last_of("\\") + 1);
				ratio_ = SequenceMatcher(str_proc, rec_text).ratio();
				if (ratio_ > 0.80)
				{
					found = true;
					if (ratio_ > best)
					{
						best = ratio_;
						filename_chosen = entry.path().string();
					}
				}
			}

			SetWindowPos(hWnd_console, HWND_TOPMOST, rect_c.left, rect_c.top, width_c, height_c, SWP_SHOWWINDOW);
			SetWindowPos(hWnd_console, HWND_NOTOPMOST, rect_c.left, rect_c.top, width_c, height_c, SWP_SHOWWINDOW);

			SetForegroundWindow(hWnd_console);

			if (found)
			{
				cout << "Detected filename: " << filename_chosen << "\nIs it right? [Y/n]: ";
				cin.ignore();
				getline(cin, okay);
				if (okay[0] == 'n')
				{
					cout << "Insert the correct filename (only filename, no path): ";
					getline(cin, filename_chosen);
					filename_chosen = filename_current + filename_chosen;
					if (!(filename_chosen.find(".json") != string::npos))
						filename_chosen += ".json";
				}

			}
			else
			{
				cout << "No filename similar to the one detected.\nInsert the correct filename (only filename, no path): ";
				cin.ignore();
				getline(cin, filename_chosen);
				filename_chosen = filename_current + filename_chosen;
				if (!(filename_chosen.find(".json") != string::npos))
					filename_chosen += ".json";
			}

			SetWindowPos(hWnd_found, HWND_TOPMOST, rect.left, rect.top, width, height, SWP_SHOWWINDOW);
			SetWindowPos(hWnd_found, HWND_NOTOPMOST, rect.left, rect.top, width, height, SWP_SHOWWINDOW);

			if (!SetForegroundWindow(hWnd_found))
				continue;

			in.open(filename_chosen);
			if (in.fail())
			{
				cout << "File " << filename_chosen << " was not found!" << endl;
				continue;
			}

			file = json::parse(in);
			in.close();

			inRange(image_hsv, DN_GRN, UP_GRN, image_proc);
			erode(image_proc, image_proc, ELEMENT);
			dilate(image_proc, image_proc, ELEMENT);

			findContours(image_proc, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
			if (!contours.empty() && isBox(contours, 150))
			{
				r_confirm = biggestRect(contours);
			}
			else
			{
				cout << "Unable to detect Confirm button" << endl;
				continue;
			}

			inRange(image_hsv, DN_PPL, UP_PPL, image_proc);
			erode(image_proc, image_proc, ELEMENT);
			dilate(image_proc, image_proc, ELEMENT);

			findContours(image_proc, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
			if (!contours.empty() && isBox(contours, 350))
			{
				r_bonus = biggestRect(contours);
			}
			else
			{
				cout << "Unable to detect Bonus time button. Will not be used" << endl;
			}

			writeAnswers(file, r_confirm, r_bonus, rect, ms, thr, scrw, scrh);
			
		}
		else
		{
			cout << "No textbox detected" << endl;
		}
		
	}

	return 0;

}

void mouseClick(int x, int y, int scrw, int scrh)
{
	INPUT input = {};

	input.type = INPUT_MOUSE;
	input.mi.mouseData = 0;
	input.mi.dx = (x * 65536) / scrw;
	input.mi.dy = (y * 65536) / scrh;
	input.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE | MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP;
	SendInput(1, &input, sizeof(input));
}

void writeAnswers(json arr, Rect r_confirm, Rect r_bonus, RECT r, int ms, int thr, int scrw, int scrh)
{
	time_t start, current;
	INPUT input;
	int i, len, thresh, x_conf, y_conf, x_bon, y_bon;
	bool bonus;
	float progress;

	x_conf = r.left + r_confirm.x + r_confirm.width/2;
	y_conf = r.top + r_confirm.y + r_confirm.height/2;

	bonus = !r_bonus.empty();
	if (bonus)
	{
		x_bon = r.left + r_bonus.x + r_bonus.width/2;
		y_bon = r.top + r_bonus.y + r_bonus.height/2;
	}

	thresh = THRESH;
	len = arr.size();

	cout << "To be written: " << len << endl;

	time(&start);
	for (i = 0; i < len; i++)
	{
		//cout << len - i << endl;
		progress = (float(i) / float(len)) * 10;
		cout << "[" << string(int(progress*3), '#') << string(30 - int(progress*3), ' ') << "] " << int(progress * 10) << "%" << "\r" << flush;

		pressKeys(arr[i]);
		Sleep(ms);
		mouseClick(x_conf, y_conf, scrw, scrh);
		Sleep(ms);

		time(&current);
		if ((current - start) > thresh && (len - i + 1) > 45 && bonus)
		{
			mouseClick(x_bon, y_bon, scrw, scrh);
			time(&start);
			thresh = thr;
		}
	}

	cout << "[##############################] 100%" << endl << flush;
}

void pressKeys(string s)
{
	INPUT ip;
	ip.type = INPUT_KEYBOARD;
	ip.ki.wScan = 0;
	ip.ki.time = 0;
	ip.ki.dwExtraInfo = 0;
	HKL kbl = GetKeyboardLayout(0);
	for (unsigned int i = 0; i < s.length(); ++i)
	{
		unsigned char c = s[i];
		switch (c)
		{

		}
		ip.ki.wVk = VkKeyScanExA(c, kbl);
		ip.ki.dwFlags = 0;
		SendInput(1, &ip, sizeof(ip));
		ip.ki.dwFlags = KEYEVENTF_KEYUP;
		SendInput(1, &ip, sizeof(ip));
	}
}

bool isBox(vector<vector<Point>> contours, int thresh)
{
	int i;
	for (i = 0; i < contours.size(); i++)
	{
		if (contourArea(contours[i]) > thresh)
			return 1;
	}

	return 0;
}

Rect biggestRect(vector<vector<Point>> contours)
{
	int i, max = 0, c;
	vector<Point> contour = contours[0];
	for (i = 0; i < contours.size(); i++)
	{
		c = contourArea(contours[i]);
		if (c > max)
		{
			contour = contours[i];
			max = c;
		}
	}

	return boundingRect(contour);
}

string strToLower(string str)
{
	int i;

	for (i = 0; i < str.length(); i++)
	{
		str[i] = tolower(str[i]);
	}

	return str;
}

int inLanguages(string str)
{
	int i;
	for (i = 0; i < LEN_LANGS; i++)
	{
		if (!str.compare(LANGUAGES[i]))
		{
			return 1;
		}
	}

	return 0;
}

WORD getCorrKey(char c)
{
	int i;
	c = tolower(c);
	for (i = 0; i < LEN_VKEYS; i++)
	{
		if (VKEYS[i].c == c)
		{
			return VKEYS[i].vKey;
		}
	}
	cout << "WARNING: given character was't mapped successfully" << endl;
	return 0x00;
}

void keyPress(WORD keyCode)
{
	INPUT input;
	input.type = INPUT_KEYBOARD;
	input.ki.wScan = MapVirtualKey(keyCode, 0);
	input.ki.wVk = keyCode;
	input.ki.dwFlags = KEYEVENTF_SCANCODE;
	SendInput(1, &input, sizeof(INPUT));
}

void keyRelease(WORD keyCode)
{
	INPUT input;
	input.type = INPUT_KEYBOARD;
	input.ki.wScan = MapVirtualKey(keyCode, 0);
	input.ki.wVk = keyCode;
	input.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
	SendInput(1, &input, sizeof(INPUT));
}

Mat capture(HWND hWnd_found, int width, int height, RECT rect) {
	HDC hScreen, hCompatibleDC;
	HBITMAP hbwindow;
	BITMAPINFOHEADER bi;
	Mat src;

	hScreen = GetWindowDC(hWnd_found);
	hCompatibleDC = CreateCompatibleDC(hScreen);
	SetStretchBltMode(hCompatibleDC, COLORONCOLOR);

	src.create(height, width, CV_8UC4);

	hbwindow = CreateCompatibleBitmap(hScreen, width, height);
	bi.biSize = sizeof(BITMAPINFOHEADER);
	bi.biWidth = width;
	bi.biHeight = -height;
	bi.biPlanes = 1;
	bi.biBitCount = 32;
	bi.biCompression = BI_RGB;
	bi.biSizeImage = 0;
	bi.biXPelsPerMeter = 0;
	bi.biYPelsPerMeter = 0;
	bi.biClrUsed = 0;
	bi.biClrImportant = 0;

	SelectObject(hCompatibleDC, hbwindow);
	StretchBlt(hCompatibleDC, 0, 0, width, height, hScreen, 0, 0, width, height,  SRCCOPY);
	GetDIBits(hCompatibleDC, hbwindow, 0, height, src.data, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

	DeleteObject(hbwindow);
	DeleteDC(hCompatibleDC);
	ReleaseDC(hWnd_found, hScreen);

	return src;
}

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
	char title[MAX_LENGTH];
	GetWindowTextA(hwnd, title, sizeof(title));
	//cout << title << endl;
	if (!strcmp(WND_NAME_1, title) || !strcmp(WND_NAME_2, title)) {
		hWnd_found = hwnd;
		return FALSE;
	}

	return TRUE;
}