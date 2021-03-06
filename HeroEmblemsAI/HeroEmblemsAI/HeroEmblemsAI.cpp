// HeroEmblemsAI.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "HeroEmblemsAI.h"
#include <Winuser.h>
#include <atlimage.h> 
#include <string>
#include "search.h"
#include <opencv2/core/core.hpp> 
#include <opencv2/highgui/highgui.hpp> 

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

using namespace std;
using namespace cv;

const int ROW = 7;
const int COL = 8;

Mat J,X,D,A;

struct Point {
	int x = 0;
	int y = 0;
	int role = 0; //0->unknown, 1->剑, 2->星星, 3->盾, 4->爱心
};


int map[7][8];
Mat CImageToMat(CImage& cimage)
{
	Mat mat;

	int nChannels = cimage.GetBPP() / 8;

	int nWidth = cimage.GetWidth();
	int nHeight = cimage.GetHeight();


	//重建mat  
	if (1 == nChannels)
	{
		mat.create(nHeight, nWidth, CV_8UC1);
	}
	else if (3 == nChannels)
	{
		mat.create(nHeight, nWidth, CV_8UC3);
	}
	else if (4 == nChannels)
	{
		mat.create(nHeight, nWidth, CV_8UC4);
	}


	//拷贝数据  


	uchar* pucRow;                                  //指向数据区的行指针  
	uchar* pucImage = (uchar*)cimage.GetBits();     //指向数据区的指针  
	int nStep = cimage.GetPitch();                  //每行的字节数,注意这个返回值有正有负  


	for (int nRow = 0; nRow < nHeight; nRow++)
	{
		pucRow = (mat.ptr<uchar>(nRow));
		for (int nCol = 0; nCol < nWidth; nCol++)
		{
			if (1 == nChannels)
			{
				pucRow[nCol] = *(pucImage + nRow * nStep + nCol);
			}
			else if (3 == nChannels)
			{
				for (int nCha = 0; nCha < 3; nCha++)
				{
					pucRow[nCol * 3 + nCha] = *(pucImage + nRow * nStep + nCol * 3 + nCha);
				}
			}
		}
	}
	
	return mat;
}
double getSimilarity(const Mat A, const Mat B) {
	if (A.rows > 0 && A.rows == B.rows && A.cols > 0 && A.cols == B.cols) {
		// Calculate the L2 relative error between images.
		double errorL2 = norm(A, B, CV_L2);
		// Convert to a reasonable scale, since L2 error is summed across all pixels of the image.
		double similarity = errorL2 / (double)(A.rows * A.cols);
		return similarity;
	}
	else {
		//Images have a different size
		return 100000000.0;  // Return a bad value
	}
}
void WriteBmpToFile(HBITMAP hBitmap, LPCTSTR path) {
	HDC hDC = ::CreateDC(_T("DISPLAY"), NULL, NULL, NULL);
	int iBits = ::GetDeviceCaps(hDC, BITSPIXEL) * ::GetDeviceCaps(hDC, PLANES);//当前分辨率下每个像素所占字节数    
	::DeleteDC(hDC);

	WORD   wBitCount;   //位图中每个像素所占字节数      
	if (iBits <= 1)
		wBitCount = 1;
	else if (iBits <= 4)
		wBitCount = 4;
	else if (iBits <= 8)
		wBitCount = 8;
	else if (iBits <= 24)
		wBitCount = 24;
	else
		wBitCount = iBits;

	DWORD   dwPaletteSize = 0;    //调色板大小， 位图中像素字节大小   
	if (wBitCount <= 8)
		dwPaletteSize = (1 << wBitCount) *    sizeof(RGBQUAD);


	BITMAP  bm;        //位图属性结构  
	::GetObject(hBitmap, sizeof(bm), (LPSTR)&bm);


	BITMAPINFOHEADER   bi;       //位图信息头结构       
	bi.biSize = sizeof(BITMAPINFOHEADER);
	bi.biWidth = bm.bmWidth;
	bi.biHeight = bm.bmHeight;
	bi.biPlanes = 1;
	bi.biBitCount = wBitCount;
	bi.biCompression = BI_RGB; //BI_RGB表示位图没有压缩  
	bi.biSizeImage = 0;
	bi.biXPelsPerMeter = 0;
	bi.biYPelsPerMeter = 0;
	bi.biClrUsed = 0;
	bi.biClrImportant = 0;

	DWORD dwBmBitsSize = ((bm.bmWidth * wBitCount + 31) / 32) * 4 * bm.bmHeight;
	HANDLE hDib = ::GlobalAlloc(GHND, dwBmBitsSize + dwPaletteSize + sizeof(BITMAPINFOHEADER));  //为位图内容分配内存  
	LPBITMAPINFOHEADER lpbi = (LPBITMAPINFOHEADER)GlobalLock(hDib);
	*lpbi = bi;

	HANDLE hPal = ::GetStockObject(DEFAULT_PALETTE);  // 处理调色板   
	HANDLE  hOldPal = NULL;
	if (hPal)
	{
		hDC = ::GetDC(NULL);
		hOldPal = SelectPalette(hDC, (HPALETTE)hPal, FALSE);
		RealizePalette(hDC);
	}
	::GetDIBits(hDC, hBitmap, 0, (UINT)bm.bmHeight, (LPSTR)lpbi + sizeof(BITMAPINFOHEADER) + dwPaletteSize, (BITMAPINFO*)lpbi, DIB_RGB_COLORS);// 获取该调色板下新的像素值  
	if (hOldPal)//恢复调色板  
	{
		SelectPalette(hDC, (HPALETTE)hOldPal, TRUE);
		RealizePalette(hDC);
		::ReleaseDC(NULL, hDC);
	}

	BITMAPFILEHEADER   bmfHdr; //位图文件头结构       
	bmfHdr.bfType = 0x4D42;  // "BM"    // 设置位图文件头  
	DWORD dwDIBSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + dwPaletteSize + dwBmBitsSize;
	bmfHdr.bfSize = dwDIBSize;
	bmfHdr.bfReserved1 = 0;
	bmfHdr.bfReserved2 = 0;
	bmfHdr.bfOffBits = (DWORD)sizeof(BITMAPFILEHEADER) + (DWORD)sizeof(BITMAPINFOHEADER) + dwPaletteSize;

	HANDLE hFile = CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);//创建位图文件     
	DWORD dwWritten;
	WriteFile(hFile, (LPSTR)&bmfHdr, sizeof(BITMAPFILEHEADER), &dwWritten, NULL);   // 写入位图文件头  
	WriteFile(hFile, (LPSTR)lpbi, dwDIBSize, &dwWritten, NULL);// 写入位图文件其余内容  

	GlobalUnlock(hDib);   //清除     
	GlobalFree(hDib);
	CloseHandle(hFile);
}

void cropImage(HBITMAP input) {
	CImage tem;
	tem.Attach(input);
	double width = tem.GetWidth();
	double height = tem.GetHeight();

	for (int row = 0; row < 7; row++) {
		for (int col = 0; col < 8; col++) {
			CImage piece;



			piece.Create(width / 8, height / 7, tem.GetBPP());
			tem.BitBlt(piece.GetDC(), 0, 0, width / 8, height / 7, width / 8 * col, height / 7 * row, SRCCOPY);

			WriteBmpToFile(piece,_T("tem.bmp"));
			Mat piece_mat = imread("tem.bmp");

			double j = getSimilarity(J, piece_mat);
			double x = getSimilarity(X, piece_mat);
			double d = getSimilarity(D, piece_mat);
			double a = getSimilarity(A, piece_mat);

			if (j < 1.5) {
				map[row][col] = 1;
			} else if (x < 1.5) {
				map[row][col] = 2;
			} else if(d < 1.5) {
				map[row][col] = 3;
			} else if (a < 1.5) {
				map[row][col] = 4;
			} else {
				map[row][col] = 0;
			}

			string s = "out/" + to_string(row) + to_string(col) + ".bmp";
			std::wstring stemp = std::wstring(s.begin(), s.end());
			LPCWSTR sw = stemp.c_str();


			WriteBmpToFile(piece, sw);
			piece.ReleaseDC();
			piece.Destroy();
		}
	}
}

HBITMAP getScreenshotByHWND(HWND hWnd) {
	
	double dy = 0.5376;
	double dx = 0.9724;

	HDC hScreenDC = ::GetDC(hWnd);
	HDC MemDC = ::CreateCompatibleDC(hScreenDC);
	RECT rect;
	::GetWindowRect(hWnd, &rect);
	SIZE screensize;
	screensize.cx = rect.right - rect.left;
	screensize.cy = rect.bottom - rect.top;
	HBITMAP hBitmap = ::CreateCompatibleBitmap(hScreenDC, screensize.cx*dx*0.9773, screensize.cy*dy*0.83498);
	HGDIOBJ hOldBMP = ::SelectObject(MemDC, hBitmap);

	::BitBlt(MemDC, 0, 0, screensize.cx*dx, screensize.cy*(1-dy), hScreenDC, screensize.cx*(1-dx), screensize.cy*dy, SRCCOPY);
	cout << rect.left << " " << rect.top << endl;
	::SelectObject(MemDC, hOldBMP);
	::DeleteObject(MemDC);
	::ReleaseDC(hWnd, hScreenDC);
	return hBitmap;
}

void printMap() {
	for(int r = 0; r < ROW; r++) {
		for (int c = 0; c < COL; c++) {
			cout << map[r][c];
		}
		cout << endl;
	}
}

int evalue() {
	//printMap();
	for (int r = 0; r < ROW; r++) {
		int count = 0;
		int role = -1;
		for (int c = 0; c < COL; c++) {
			if (map[r][c] == role) {
				count++;
				if (count == 3) {
					cout << "Yes" << endl;
					return 1;
				}
			}
			else {
				count = 1;
				role = map[r][c];
			}
		}
	}
	for (int c = 0; c < COL; c++) {
		int count = 0;
		int role = -1;
		for (int r = 0; r < ROW; r++) {
			if (map[r][c] == role) {
				count++;
				if (count == 3) {
					cout << "Yes" << endl;
					return 1;
				}
			}
			else {
				count = 1;
				role = map[r][c];
			}
		}
	}
	cout << "No" << endl;
	return 0;
}

int search(int x1, int y1, int x2, int y2) {
	if (x1 < COL && y1 < ROW && x2 < COL && y2 < ROW) {
		int tem1 = map[y1][x1];
		int tem2 = map[y2][x2];
		map[y1][x1] = tem2;
		map[y2][x2] = tem1;
		int rtn = evalue();
		map[y1][x1] = tem1;
		map[y2][x2] = tem2;
		return rtn;
	}
	else return 0; //point out of bound
}

void searchAll() {
	for (int x = 0; x < COL; x++) {
		for (int y = 0; y < ROW; y++) {
			if (search(x, y, x, y + 1) > 0) { //swap down;
				cout << x << "," << y << "down" << endl;
				return;
			}
			if (search(x, y, x + 1, y) > 0) { //swap right;
				cout << x << "," << y << "right" << endl;
				return;
			}
		}
	}
}



int main() {
	J = imread("J.bmp");
	X = imread("X.bmp");
	D = imread("D.bmp");
	A = imread("A.bmp");
	
	/*
	Mat img = imread("C:/Users/Mingchao/Pictures/1.jpg");
	namedWindow("a");
	imshow("b", img);
	waitKey(6000);
	*/
	HWND hWnd;

	hWnd = ::FindWindow(_T("CHWindow"), NULL);
	//hWnd = ::FindWindow(_T("CHWindow"),_T("AIRPLAYER"));

	HBITMAP screenshot = getScreenshotByHWND(hWnd);
	WriteBmpToFile(screenshot, _T("bbc.bmp"));
	cropImage(screenshot);

	printMap();
	cout << endl;
	searchAll();
	HWND edit = (HWND)0x00020A10;
	
	
	int a1 =  PostMessage(edit, WM_LBUTTONDOWN, 0, MAKELPARAM(200, 200));
	int a2 = PostMessage(edit, WM_MOUSEMOVE, 0, MAKELPARAM(300, 300));

	int a3 = PostMessage(edit, WM_LBUTTONUP, 0, MAKELPARAM(300, 300));
	cout << a1 << " " << a2 << " " << a3 << endl;
	return 0;
}