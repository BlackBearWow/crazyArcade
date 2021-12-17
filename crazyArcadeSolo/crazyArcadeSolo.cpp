#include "framework.h"
#include "crazyArcadeSolo.h"
#include <iostream>
#include <random>
#include <vector>
#include <mmsystem.h>
#include <WinSock2.h>
#include <Ole2.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus")
#pragma comment(lib, "winmm.lib")   //사운드 라이브러리
#pragma comment(lib, "ws2_32.lib")  //윈속 라이브러리
#define MAX_LOADSTRING 100

//client영역은 800x600 인게임영역은 600x520 
//인게임일때 가로공백 20 세로공백 41
//기본적으로 한칸에 40x40임. 박스같은건 40x44. 40보다 큰만큼 위로 튀어나오는거임.

#define CLIENTWIDTHSIZE     800
#define CLIENTHEIGHTSIZE    600
#define INGAMEWIDTHSIZE     600
#define INGAMEHEIGHTSIZE    520
#define WIDTHGAP    20
#define HEIGHTGAP   41
#define MAPWIDTH    15
#define MAPHEIGHT   13

#define POPTIME     0.5 //터지고나서 사라질때까지의 시간. 물풍선 또는 블록

#define SPEEDADD    15  //스피드아이템 먹었을때 추가되는 스피드

using namespace std;
using namespace Gdiplus;

//내가정의한 구조체:
typedef struct _tagFPoint
{
    float x, y;
} FPoint;

union keyPress {
    short data = 0;
    char arr[2];
};

typedef struct _tagKeyboardDown
{
    union keyPress left, up, right, down;
    int direction = 3;  //0 left 1 up 2 right 3 down
} KeyboardDown;

typedef struct _tagMainCharacter
{
    FPoint where;
    POINT position;
    vector<Image*> leftImage, upImage, rightImage, downImage, inWaterBalloonImage, deadImage;
    int inWaterBalloon = false;
    float inWaterBalloonTime = 0.0;
    int dead = false;
    int speed = 150;
    int sight = 3;      //0 left 1 up 2 right 3 down
    float walkingTime = 0;
    int waterballoonSize = 1;   //물줄기 길이
    int waterballoonMaxNum = 1;    //최대 물풍선 개수
    int waterballoonNum = 0;    //현재 물풍선 개수
    int waterballoonCount = 0;  //waterballoon에게 넘겨줄 번호. 0~99로서 물풍선 놓을때마다 1씩 커짐
} MainCharacter;

typedef struct _tagBackground
{
    Image* readyImage = NULL;
    Image* ready1 = NULL;
    Image* ready2 = NULL;
    Image* winImage = NULL;
    Image* inGameImage = NULL;
    Image* map1Image = NULL;
    int ready = 1;
    int winner = 0;
    float closeTime = 0.0;
    float readyMouseTime = 0;
} Background;

typedef struct _tagMapInfo
{
    int info[MAPHEIGHT][MAPWIDTH];      //무슨 블록인지 저장해둔다.
    float time[MAPHEIGHT][MAPWIDTH];    //아이템 시간 저장.
} MapInfo;

typedef struct _tagSoftBlockMapInfo
{
    int info[MAPHEIGHT][MAPWIDTH];      //무슨 블록인지 저장해둔다.
    int item[MAPHEIGHT][MAPWIDTH];      //아이템 정보를 저장해둔다.
    int pop[MAPHEIGHT][MAPWIDTH];       //터지는중이면1 터졌거나 멀쩡하다면0
    float poptime[MAPHEIGHT][MAPWIDTH]; //POPTIME보다 커지면 터지게 한다.
} SoftBlockMapInfo;

typedef struct _tagBalloonInfo
{
    bool pop = false;
    //POINT where;
    float waitingtTime = 0.0;
    float boomTime = 0.0;
    int length = 1;
} BalloonInfo;

typedef struct _tagWaterBalloon
{
    vector<Image*> image;
    vector<Image*> boomImage;
    vector<BalloonInfo> balloon;
} WaterBalloon;

typedef struct _tagOneWaterBalloonMapInfo
{
    int player = 0;
    int what;
    BalloonInfo* info;
} OneWaterBalloonMapInfo;

typedef struct _tagWaterBalloonMapInfo
{
    OneWaterBalloonMapInfo info[MAPHEIGHT][MAPWIDTH];
} WaterBalloonMapInfo;

typedef struct _tagHardBlock
{
    Image* house1 = NULL;
    Image* house2 = NULL;
    Image* tree = NULL;
} HardBlock;

typedef struct _tagSoftBlock
{
    Image* redBlock = NULL;
    Image* redBlockPop1 = NULL;
    Image* redBlockPop2 = NULL;
    Image* orangeBlock = NULL;
    Image* orangeBlockPop1 = NULL;
    Image* orangeBlockPop2 = NULL;
    Image* brownBlock = NULL;
    Image* brownBlockPop1 = NULL;
    Image* brownBlockPop2 = NULL;
} SoftBlock;

typedef struct _tagItemBlock
{
    Image* speed1 = NULL;
    Image* speed2 = NULL;
    Image* waterLength1 = NULL;
    Image* waterLength2 = NULL;
    Image* waterBalloon1 = NULL;
    Image* waterBalloon2 = NULL;
} ItemBlock;

// 전역 변수:
HINSTANCE hInst;                                // 현재 인스턴스입니다.
WCHAR szTitle[MAX_LOADSTRING];                  // 제목 표시줄 텍스트입니다.
WCHAR szWindowClass[MAX_LOADSTRING];            // 기본 창 클래스 이름입니다.

bool g_loop = true;
HWND g_hWnd;
HDC g_hDC;
HDC backMemDC;  //더블버퍼링
HBITMAP backBitmap = NULL;
HBITMAP hMyBitmap, holdBitmap;
POINT ptMouse;  //마우스 위치 저장

LARGE_INTEGER g_tSecond;
LARGE_INTEGER g_tTime;
LARGE_INTEGER g_tTime2;
float g_fDeltatime; //컴성능에 좌우되지 않게 초에 따라 움직이게하는 변수

MainCharacter player1, player2;
Background background;

MapInfo* hardBlock = NULL;      //안깨지는블럭
SoftBlockMapInfo* softBlock = NULL;      //깨지는블럭
MapInfo* itemBlock = NULL;      //아이템맵
WaterBalloonMapInfo* waterBalloonBlock = NULL;   //물풍선블럭

HardBlock hardBlockImage;
SoftBlock softBlockImage;
ItemBlock itemBlockImage;

WaterBalloon waterBalloon;  //물풍선 정보들의 구조체

KeyboardDown player1key, player2key;  //어떤 키를 눌렀나 확인

int playerNum = 0;
SOCKET hSock;

TCHAR info[50];     //코딩할때 점검하기 위해 만든 문자열

// 이 코드 모듈에 포함된 함수의 선언을 전달합니다:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

void Error(LPCWSTR m);
void Run();

void accessServer();

void timeCount();
void initBackground();
void initCharacter();
void initBlockImage();

void readyDo();
void initInGame();
void makeBlock();

void inGameDo();
void mainCharacterMovePlayer1();
void mainCharacterMovePlayer2();
void pressBalloonPlayer1(int vKey);
void pressBalloonPlayer2(int vKey);
bool canSetWaterBalloonPlayer1(int x, int y);
bool canSetWaterBalloonPlayer2(int x, int y);
void setWaterBalloonPlayer1(int x, int y);
void setWaterBalloonPlayer2(int x, int y);
void getItemPlayer1();
void getItemPlayer2();
bool IsThereEmpty(int x, int y);
void balloonTimeAndPop();
void balloonPop(int x, int y);
void softBlockPop();
void itemTimeflow();
void touchWaterBalloonPlayer1();
void touchWaterBalloonPlayer2();
void inWaterBalloonDoPlayer1();
void inWaterBalloonDoPlayer2();
void whoIsWin();
void closeInGame();

void sendReady();

void drawBackground(Graphics* g);
void drawCharacterPlayer1(Graphics* g);
void drawCharacterPlayer2(Graphics* g);
void drawInfo();
void drawAll(Graphics* g);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: 여기에 코드를 입력합니다.

    // 전역 문자열을 초기화합니다.
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_CRAZYARCADESOLO, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // 애플리케이션 초기화를 수행합니다:
    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    g_hDC = GetDC(g_hWnd);

    //윈속 초기화 
    WSADATA wsadata;
    if (WSAStartup(MAKEWORD(2, 2), &wsadata) != 0)Error(L"WSAStartup() error");

    //Gdiplus 사용하기 위해 초기화 작업
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    //더블버퍼링에 필요한 비트맵 생성
    static RECT rt;
    backMemDC = CreateCompatibleDC(g_hDC);
    backBitmap = CreateCompatibleBitmap(g_hDC, CLIENTWIDTHSIZE, CLIENTHEIGHTSIZE);
    holdBitmap = (HBITMAP)SelectObject(backMemDC, backBitmap);

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_CRAZYARCADESOLO));

    MSG msg;

    initBackground();
    initCharacter();
    initBlockImage();

    QueryPerformanceFrequency(&g_tSecond);
    QueryPerformanceCounter(&g_tTime);

    //accessServer();

    mciSendString(TEXT("play sound\\lobby.wav"), NULL, 0, NULL);
    //PlaySound(TEXT("sound\\lobby.wav"), NULL, SND_FILENAME | SND_ASYNC | SND_LOOP | SND_NOSTOP);
    // 기본 메시지 루프입니다:
    while (g_loop)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            Run();
        }
    }

    return (int)msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_CRAZYARCADESOLO));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    //wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_CRAZYARCADE);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance; // 인스턴스 핸들을 전역 변수에 저장합니다.

    HWND hWnd = CreateWindowW(szWindowClass, szTitle, (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX),
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

    g_hWnd = hWnd;

    if (!hWnd)
    {
        return FALSE;
    }
    SetWindowPos(hWnd, HWND_TOPMOST, 100, 100,
        CLIENTWIDTHSIZE, CLIENTHEIGHTSIZE, SWP_NOMOVE | SWP_NOZORDER);
    RECT clientRect; GetClientRect(hWnd, &clientRect);
    SetWindowPos(hWnd, HWND_TOPMOST, 100, 100,
        CLIENTWIDTHSIZE + (CLIENTWIDTHSIZE - clientRect.right),
        CLIENTHEIGHTSIZE + (CLIENTHEIGHTSIZE - clientRect.bottom),
        SWP_NOMOVE | SWP_NOZORDER);
    //화면 사이즈 딱 맞게 조정.

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    return TRUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        // 메뉴 선택을 구문 분석합니다:
        switch (wmId)
        {
        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;
        case IDM_EXIT:
            DestroyWindow(hWnd);
            exit(0);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    break;
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        // TODO: 여기에 hdc를 사용하는 그리기 코드를 추가합니다...
        EndPaint(hWnd, &ps);
    }
    break;
    case WM_DESTROY:
        PostQuitMessage(0);
        exit(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

void Error(LPCWSTR m) {
    MessageBox(NULL, m, m, MB_OK);
    exit(1);
}

void Run()
{
    static Graphics g(backMemDC);
    timeCount();
    if (background.ready == 1)
        readyDo();
    else
        inGameDo();
    drawBackground(&g);
    if (background.ready == 1)
    {
        
    }
    else
    {
        drawAll(&g);
    }
    BitBlt(g_hDC, 0, 0, CLIENTWIDTHSIZE, CLIENTHEIGHTSIZE, backMemDC, 0, 0, SRCCOPY);
}

void accessServer()
{
    SOCKADDR_IN servAddr;
    hSock = socket(AF_INET, SOCK_STREAM, 0);
    if (hSock == INVALID_SOCKET) Error(L"socket() error");

    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    servAddr.sin_port = htons(31300);

    if (connect(hSock, (SOCKADDR*)&servAddr, sizeof(servAddr)) == SOCKET_ERROR) Error(L"connect() error");
    char message[30];
    int strLen = recv(hSock, message, sizeof(message) - 1, 0);
    if (strLen == -1) Error(L"recv() error");
    if (message[0] == '1')
        playerNum = 1;
    else
        playerNum = 2;

    DWORD recvTimeout = 5;  // 0.005초.
    setsockopt(hSock, SOL_SOCKET, SO_RCVTIMEO, (char*)&recvTimeout, sizeof(recvTimeout));
    //readyDo일때 프로그램 대기 상태로 안돌아가기 위한 조치.
}

void timeCount()
{
    QueryPerformanceCounter(&g_tTime2);
    g_fDeltatime = (g_tTime2.QuadPart - g_tTime.QuadPart) / (float)g_tSecond.QuadPart;
    g_tTime = g_tTime2;
}

void initBackground()
{
    background.readyImage = Image::FromFile(L"crazyArcadeImage\\크아대기실.png");
    background.ready1 = Image::FromFile(L"crazyArcadeImage\\크아시작1.png");
    background.ready2 = Image::FromFile(L"crazyArcadeImage\\크아시작2.png");
    background.winImage = Image::FromFile(L"crazyArcadeImage\\win.png");
    background.inGameImage = Image::FromFile(L"crazyArcadeImage\\크아인게임.png");
    background.map1Image = Image::FromFile(L"crazyArcadeImage\\크아배경1.png");
}

void initCharacter()
{
    player1.where.x = 40;
    player1.where.y = 40;

    player1.leftImage.push_back(Image::FromFile(L"crazyArcadeImage\\bazzi\\배찌01.png"));
    player1.leftImage.push_back(Image::FromFile(L"crazyArcadeImage\\bazzi\\배찌02.png"));
    player1.leftImage.push_back(Image::FromFile(L"crazyArcadeImage\\bazzi\\배찌03.png"));
    player1.leftImage.push_back(Image::FromFile(L"crazyArcadeImage\\bazzi\\배찌04.png"));
    player1.leftImage.push_back(Image::FromFile(L"crazyArcadeImage\\bazzi\\배찌05.png"));
    player1.leftImage.push_back(Image::FromFile(L"crazyArcadeImage\\bazzi\\배찌06.png"));

    player1.upImage.push_back(Image::FromFile(L"crazyArcadeImage\\bazzi\\배찌11.png"));
    player1.upImage.push_back(Image::FromFile(L"crazyArcadeImage\\bazzi\\배찌12.png"));
    player1.upImage.push_back(Image::FromFile(L"crazyArcadeImage\\bazzi\\배찌13.png"));
    player1.upImage.push_back(Image::FromFile(L"crazyArcadeImage\\bazzi\\배찌14.png"));
    player1.upImage.push_back(Image::FromFile(L"crazyArcadeImage\\bazzi\\배찌15.png"));
    player1.upImage.push_back(Image::FromFile(L"crazyArcadeImage\\bazzi\\배찌16.png"));

    player1.rightImage.push_back(Image::FromFile(L"crazyArcadeImage\\bazzi\\배찌21.png"));
    player1.rightImage.push_back(Image::FromFile(L"crazyArcadeImage\\bazzi\\배찌22.png"));
    player1.rightImage.push_back(Image::FromFile(L"crazyArcadeImage\\bazzi\\배찌23.png"));
    player1.rightImage.push_back(Image::FromFile(L"crazyArcadeImage\\bazzi\\배찌24.png"));
    player1.rightImage.push_back(Image::FromFile(L"crazyArcadeImage\\bazzi\\배찌25.png"));
    player1.rightImage.push_back(Image::FromFile(L"crazyArcadeImage\\bazzi\\배찌26.png"));

    player1.downImage.push_back(Image::FromFile(L"crazyArcadeImage\\bazzi\\배찌31.png"));
    player1.downImage.push_back(Image::FromFile(L"crazyArcadeImage\\bazzi\\배찌32.png"));
    player1.downImage.push_back(Image::FromFile(L"crazyArcadeImage\\bazzi\\배찌33.png"));
    player1.downImage.push_back(Image::FromFile(L"crazyArcadeImage\\bazzi\\배찌34.png"));
    player1.downImage.push_back(Image::FromFile(L"crazyArcadeImage\\bazzi\\배찌35.png"));
    player1.downImage.push_back(Image::FromFile(L"crazyArcadeImage\\bazzi\\배찌36.png"));

    player1.inWaterBalloonImage.push_back(Image::FromFile(L"crazyArcadeImage\\bazzi\\inWaterBalloon1.png"));
    player1.inWaterBalloonImage.push_back(Image::FromFile(L"crazyArcadeImage\\bazzi\\inWaterBalloon2.png"));

    player1.deadImage.push_back(Image::FromFile(L"crazyArcadeImage\\bazzi\\dead.png"));
    player1.deadImage.push_back(Image::FromFile(L"crazyArcadeImage\\bazzi\\dead.png"));
}

void initBlockImage()
{
    softBlockImage.redBlock = Image::FromFile(L"crazyArcadeImage\\softBlock\\크아빨간블럭.png");
    softBlockImage.redBlockPop1 = Image::FromFile(L"crazyArcadeImage\\softBlock\\크아빨간블럭1.png");
    softBlockImage.redBlockPop2 = Image::FromFile(L"crazyArcadeImage\\softBlock\\크아빨간블럭2.png");
    softBlockImage.orangeBlock = Image::FromFile(L"crazyArcadeImage\\softBlock\\크아주황블럭.png");
    softBlockImage.orangeBlockPop1 = Image::FromFile(L"crazyArcadeImage\\softBlock\\크아주황블럭1.png");
    softBlockImage.orangeBlockPop2 = Image::FromFile(L"crazyArcadeImage\\softBlock\\크아주황블럭2.png");
    softBlockImage.brownBlock = Image::FromFile(L"crazyArcadeImage\\softBlock\\크아박스.png");
    softBlockImage.brownBlockPop1 = Image::FromFile(L"crazyArcadeImage\\softBlock\\크아박스1.png");
    softBlockImage.brownBlockPop2 = Image::FromFile(L"crazyArcadeImage\\softBlock\\크아박스2.png");

    hardBlockImage.house1 = Image::FromFile(L"crazyArcadeImage\\hardBlock\\집1.png");
    hardBlockImage.house2 = Image::FromFile(L"crazyArcadeImage\\hardBlock\\집2.png");
    hardBlockImage.tree = Image::FromFile(L"crazyArcadeImage\\hardBlock\\나무.png");

    itemBlockImage.speed1 = Image::FromFile(L"crazyArcadeImage\\item\\speed1.png");
    itemBlockImage.speed2 = Image::FromFile(L"crazyArcadeImage\\item\\speed2.png");
    itemBlockImage.waterLength1 = Image::FromFile(L"crazyArcadeImage\\item\\물줄기1.png");
    itemBlockImage.waterLength2 = Image::FromFile(L"crazyArcadeImage\\item\\물줄기2.png");
    itemBlockImage.waterBalloon1 = Image::FromFile(L"crazyArcadeImage\\item\\물풍선1.png");
    itemBlockImage.waterBalloon2 = Image::FromFile(L"crazyArcadeImage\\item\\물풍선2.png");

    waterBalloon.image.push_back(Image::FromFile(L"crazyArcadeImage\\waterBalloon\\waterBalloon0.png"));
    waterBalloon.image.push_back(Image::FromFile(L"crazyArcadeImage\\waterBalloon\\waterBalloon1.png"));
    waterBalloon.image.push_back(Image::FromFile(L"crazyArcadeImage\\waterBalloon\\waterBalloon2.png"));
    waterBalloon.image.push_back(Image::FromFile(L"crazyArcadeImage\\waterBalloon\\waterBalloon3.png"));
    waterBalloon.image.push_back(Image::FromFile(L"crazyArcadeImage\\waterBalloon\\waterBalloon4.png"));
    waterBalloon.image.push_back(Image::FromFile(L"crazyArcadeImage\\waterBalloon\\waterBalloon5.png"));
    waterBalloon.image.push_back(Image::FromFile(L"crazyArcadeImage\\waterBalloon\\waterBalloon6.png"));
    waterBalloon.image.push_back(Image::FromFile(L"crazyArcadeImage\\waterBalloon\\waterBalloon7.png"));

    waterBalloon.boomImage.push_back(Image::FromFile(L"crazyArcadeImage\\waterBalloon\\left펑1.png"));
    waterBalloon.boomImage.push_back(Image::FromFile(L"crazyArcadeImage\\waterBalloon\\left펑2.png"));
    waterBalloon.boomImage.push_back(Image::FromFile(L"crazyArcadeImage\\waterBalloon\\up펑1.png"));
    waterBalloon.boomImage.push_back(Image::FromFile(L"crazyArcadeImage\\waterBalloon\\up펑2.png"));
    waterBalloon.boomImage.push_back(Image::FromFile(L"crazyArcadeImage\\waterBalloon\\right펑1.png"));
    waterBalloon.boomImage.push_back(Image::FromFile(L"crazyArcadeImage\\waterBalloon\\right펑2.png"));
    waterBalloon.boomImage.push_back(Image::FromFile(L"crazyArcadeImage\\waterBalloon\\down펑1.png"));
    waterBalloon.boomImage.push_back(Image::FromFile(L"crazyArcadeImage\\waterBalloon\\down펑2.png"));
    waterBalloon.boomImage.push_back(Image::FromFile(L"crazyArcadeImage\\waterBalloon\\mid펑1.png"));
    waterBalloon.boomImage.push_back(Image::FromFile(L"crazyArcadeImage\\waterBalloon\\mid펑2.png"));
}

void readyDo()
{
    GetCursorPos(&ptMouse);
    ScreenToClient(g_hWnd, &ptMouse);
    if (GetAsyncKeyState(VK_F5) & 0x8000)
    {
        initInGame();
    }
    else if ((516 <= ptMouse.x) && (498 <= ptMouse.y) && (ptMouse.x <= 700) && (ptMouse.y <= 546))
    {
        if (background.readyMouseTime == 0.0)
            PlaySound(TEXT("sound\\물풍선.wav"), NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
        background.readyMouseTime += g_fDeltatime;
        if (GetAsyncKeyState(VK_LBUTTON) & 0x8000)
        {
            initInGame();
        }
    }
    else
    {
        background.readyMouseTime = 0.0;
    }
    static char message[30] = { 0, };
    static int strLen;
    strLen = recv(hSock, message, sizeof(message) - 1, 0);
    if (strLen != SOCKET_ERROR)   //전송받는데 이상이 없었다면
    {
        if (message[0] == 's')
            initInGame();
    }
}

void initInGame()
{
    background.ready = 0;
    background.winner = 0;
    background.closeTime = 0.0;
    player1.inWaterBalloon = false;     player2.inWaterBalloon = false;
    player1.inWaterBalloonTime = 0.0;   player2.inWaterBalloonTime = 0.0;
    player1.dead = false;               player2.dead = false;
    player1.speed = 150;                player2.speed = 150;
    player1.waterballoonSize = 1;       player2.waterballoonSize = 1;
    player1.waterballoonMaxNum = 1;     player2.waterballoonMaxNum = 1;
    player1.waterballoonNum = 0;        player2.waterballoonNum = 0;
    if (playerNum == 1) {
        player1.where.x = 40;
        player1.where.y = 40;
        player2.where.x = 40 * 13;
        player2.where.y = 40 * 11;
    }
    else {
        player1.where.x = 40 * 13;
        player1.where.y = 40 * 11;
        player2.where.x = 40;
        player2.where.y = 40;
    }
    player1.position.x = (int)floor(player1.where.x / 40);
    player1.position.y = (int)floor(player1.where.y / 40);
    player2.position.x = (int)floor(player2.where.x / 40);
    player2.position.y = (int)floor(player2.where.y / 40);
    hardBlock = (MapInfo*)malloc(sizeof(MapInfo));
    softBlock = (SoftBlockMapInfo*)malloc(sizeof(SoftBlockMapInfo));
    itemBlock = (MapInfo*)malloc(sizeof(MapInfo));
    waterBalloonBlock = (WaterBalloonMapInfo*)malloc(sizeof(WaterBalloonMapInfo));
    if (hardBlock != NULL)
        memset(hardBlock, 0, sizeof(MapInfo));
    if (softBlock != NULL)
        memset(softBlock, 0, sizeof(SoftBlockMapInfo));
    if (itemBlock != NULL)
        memset(itemBlock, 0, sizeof(MapInfo));
    if (waterBalloonBlock != NULL)
        memset(waterBalloonBlock, 0, sizeof(WaterBalloonMapInfo));
    for (int x = 0; x < INGAMEWIDTHSIZE / 40; x++)
    {
        for (int y = 0; y < INGAMEHEIGHTSIZE / 40; y++)
        {
            BalloonInfo* balloon = (BalloonInfo*)malloc(sizeof(BalloonInfo));
            if (balloon == NULL) exit(1);
            waterBalloonBlock->info[y][x].info = balloon;
        }
    }
    makeBlock();
    mciSendString(TEXT("stop sound\\lobby.wav"), NULL, 0, NULL);
    mciSendString(TEXT("play sound\\village.wav"), NULL, 0, NULL);
}

void makeBlock()
{
    FILE* fp = NULL;
    fp = fopen("crazyArcadeImage\\맵1", "r");
    if (fp == NULL)
        return;
    char ch;
    int randnum = 0;
    int speedProbability = 20;
    int waterLengthProbability = 40;
    int waterBalloonProbability = 60;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis(0, 99);
    for (int y = 0; y < INGAMEHEIGHTSIZE / 40; y++)
    {
        for (int x = 0; x < INGAMEWIDTHSIZE / 40; x++)
        {
            randnum = dis(gen);

            ch = fgetc(fp);
            if (ch == '\n')
                ch = fgetc(fp);
            if (ch == '0')
                softBlock->info[y][x] = 0;

            else if (ch == 'a') //빨강블럭
            {
                softBlock->info[y][x] = 100;
                if (randnum < speedProbability)
                    softBlock->item[y][x] = 1;
                else if (randnum < waterLengthProbability)
                    softBlock->item[y][x] = 2;
                else if (randnum < waterBalloonProbability)
                    softBlock->item[y][x] = 3;
            }
            else if (ch == 'b') //주황블럭
            {
                softBlock->info[y][x] = 110;
                if (randnum < speedProbability)
                    softBlock->item[y][x] = 1;
                else if (randnum < waterLengthProbability)
                    softBlock->item[y][x] = 2;
                else if (randnum < waterBalloonProbability)
                    softBlock->item[y][x] = 3;
            }
            else if (ch == 'c') //크아박스
            {
                softBlock->info[y][x] = 120;
                if (randnum < speedProbability)
                    softBlock->item[y][x] = 1;
                else if (randnum < waterLengthProbability)
                    softBlock->item[y][x] = 2;
                else if (randnum < waterBalloonProbability)
                    softBlock->item[y][x] = 3;
            }

            else if (ch == 'A')
                hardBlock->info[y][x] = 100;
            else if (ch == 'B')
                hardBlock->info[y][x] = 110;
            else if (ch == 'C')
                hardBlock->info[y][x] = 120;

            softBlock->pop[y][x] = 0;
        }
    }
    char m[30] = { 0, };
    /*
    for (int y = 0; y < INGAMEHEIGHTSIZE / 40; y++)
    {
        for (int x = 0; x < INGAMEWIDTHSIZE / 40; x++)
        {
            recv(hSock, m, sizeof(m) - 1, 0);
            if (m[0] == '1')
            {
                //여기에 메시지를 받아서 아이템 정보를 받는다.
            }
        }
    }
    */
    fclose(fp);
}

void inGameDo()
{
    GetCursorPos(&ptMouse);
    ScreenToClient(g_hWnd, &ptMouse);
    player1key.up.data = GetAsyncKeyState(VK_UP);
    player1key.down.data = GetAsyncKeyState(VK_DOWN);
    player1key.left.data = GetAsyncKeyState(VK_LEFT);
    player1key.right.data = GetAsyncKeyState(VK_RIGHT);

    player2key.up.data = GetAsyncKeyState(0x52);    //R
    player2key.down.data = GetAsyncKeyState(0x46);  //F
    player2key.left.data = GetAsyncKeyState(0x44);  //D
    player2key.right.data = GetAsyncKeyState(0x47); //G

    pressBalloonPlayer1(VK_SPACE);
    pressBalloonPlayer2(0x41);  //A

    if (background.winner == 0)
    {
        mainCharacterMovePlayer1();
        mainCharacterMovePlayer2();
        getItemPlayer1();
        getItemPlayer2();
    }
    balloonTimeAndPop();
    softBlockPop();
    itemTimeflow();
    touchWaterBalloonPlayer1();
    touchWaterBalloonPlayer2();
    inWaterBalloonDoPlayer1();
    inWaterBalloonDoPlayer2();
    whoIsWin();

    if ((647 <= ptMouse.x) && (561 <= ptMouse.y) && (ptMouse.x <= 786) && (ptMouse.y <= 592))
    {
        if (GetAsyncKeyState(VK_LBUTTON) & 0x8000)
        {
            closeInGame();
        }
    }
    //게임 나가기 버튼
}

void mainCharacterMovePlayer1()
{
    if (player1key.left.data == 0xffff8001)
        player1key.direction = 1;
    if (player1key.up.data == 0xffff8001)
        player1key.direction = 2;
    if (player1key.right.data == 0xffff8001)
        player1key.direction = 3;
    if (player1key.down.data == 0xffff8001)
        player1key.direction = 4;

    float speed = player1.speed * g_fDeltatime;
    float gox, goy;
    if ((player1key.left.data & 0x8000) && player1key.direction == 1)
    {
        player1.sight = 0;
        player1.walkingTime += g_fDeltatime;  //시간에 따라 움직이는 모양 바꾸기.
        if (fmod(player1.where.x, 40) == 0)   //왼쪽으로 움찍일때 좌우가 딱 가운데일때
        {
            if (fmod(player1.where.y, 40) == 0)   //상하도 가운데라면 왼쪽칸만 빈공간이면 움직임.
            {
                if (IsThereEmpty(floor(player1.where.x / 40) - 1, floor(player1.where.y / 40)))
                    player1.where.x -= speed;
            }
            else    //상하가 가운데가 아니라면 
            {
                if (IsThereEmpty(floor(player1.where.x / 40) - 1, floor(player1.where.y / 40)) && IsThereEmpty(floor(player1.where.x / 40) - 1, floor(player1.where.y / 40) + 1))
                {   //왼쪽 위와 아래가 둘다 뚫렸다면 움직임
                    player1.where.x -= speed;
                }
                else if ((IsThereEmpty(floor(player1.where.x / 40) - 1, floor(player1.where.y / 40)) == false) && IsThereEmpty(floor(player1.where.x / 40) - 1, floor(player1.where.y / 40) + 1))
                {   //왼쪽위는 막히고 왼쪽아래가 뚫렸을때 아래로움직임
                    player1.where.y += speed;
                    if (player1.position.y < floor(player1.where.y / 40))
                        player1.where.y = floor(player1.where.y / 40) * 40;
                }
                else if (IsThereEmpty(floor(player1.where.x / 40) - 1, floor(player1.where.y / 40)) && (IsThereEmpty(floor(player1.where.x / 40) - 1, floor(player1.where.y / 40) + 1) == false))
                {   //왼쪽아래가 막혔을때 왼쪽위로움직임
                    player1.where.y -= speed;
                    if (floor(player1.where.y / 40) < player1.position.y)
                        player1.where.y = (floor(player1.where.y / 40) + 1) * 40;
                }
            }
        }
        else
        {
            gox = player1.where.x - speed;
            if (fmod(player1.where.y, 40) == 0)   //상하가 가운데일때
            {
                if ((int)floor(gox / 40) == (int)floor(player1.where.x / 40)) //현재 캐릭터칸과 움직일 칸이 같으면 움직임
                {
                    player1.where.x = gox;
                }
                else if (IsThereEmpty(floor(gox / 40), floor(player1.where.y / 40)) == false) //현재 캐릭터칸과 움직일 칸이 다르고 빈공간이 아니라면 벽에 막히기
                {
                    player1.where.x = gox;
                    player1.where.x = (floor(player1.where.x / 40) + 1) * 40;
                }
                else    //현재 캐릭터칸과 움직일 칸이 다르고 빈공간이라면 움직이기
                    player1.where.x = gox;
            }
            else
            {
                if ((int)floor(gox / 40) == (int)floor(player1.where.x / 40)) //현재 캐릭터칸과 움직일 칸이 같으면 움직임
                {
                    player1.where.x = gox;
                }
                else if ((IsThereEmpty(floor(gox / 40), floor(player1.where.y / 40)) == false) || (IsThereEmpty(floor(gox / 40), floor(player1.where.y / 40) + 1) == false)) //현재 캐릭터칸과 움직일 칸이 다르고 빈공간이 아니라면 벽에 막히기
                {
                    player1.where.x = gox;
                    player1.where.x = (floor(player1.where.x / 40) + 1) * 40;
                }
                else    //현재 캐릭터칸과 움직일 칸이 다르고 빈공간이라면 움직이기
                    player1.where.x = gox;
            }
        }
        if (player1.where.x < 0)
            player1.where.x = 0;
    }
    else if ((player1key.up.data & 0x8000) && player1key.direction == 2)
    {
        player1.sight = 1;
        player1.walkingTime += g_fDeltatime;
        if (fmod(player1.where.y, 40) == 0)   //위로 움직일때 상하가 딱 가운데일때
        {
            if (fmod(player1.where.x, 40) == 0)
            {
                if (IsThereEmpty(floor(player1.where.x / 40), floor(player1.where.y / 40) - 1))
                    player1.where.y -= speed;
            }
            else
            {
                if (IsThereEmpty(floor(player1.where.x / 40), floor(player1.where.y / 40) - 1) && IsThereEmpty(floor(player1.where.x / 40) + 1, floor(player1.where.y / 40) - 1))
                {
                    player1.where.y -= speed;
                }
                else if ((IsThereEmpty(floor(player1.where.x / 40), floor(player1.where.y / 40) - 1) == false) && IsThereEmpty(floor(player1.where.x / 40) + 1, floor(player1.where.y / 40) - 1))
                {
                    player1.where.x += speed;
                    if (player1.position.x < floor(player1.where.x / 40))
                        player1.where.x = floor(player1.where.x / 40) * 40;
                }
                else if (IsThereEmpty(floor(player1.where.x / 40), floor(player1.where.y / 40) - 1) && (IsThereEmpty(floor(player1.where.x / 40) + 1, floor(player1.where.y / 40) - 1) == false))
                {
                    player1.where.x -= speed;
                    if (floor(player1.where.x / 40) < player1.position.x)
                        player1.where.x = (floor(player1.where.x / 40) + 1) * 40;
                }
            }
        }
        else
        {
            goy = player1.where.y - speed;
            if (fmod(player1.where.x, 40) == 0)
            {
                if ((int)floor(goy / 40) == (int)floor(player1.where.y / 40))
                    player1.where.y = goy;
                else if (IsThereEmpty(floor(player1.where.x / 40), floor(goy / 40)) == false)
                {
                    player1.where.y = goy;
                    player1.where.y = (floor(player1.where.y / 40) + 1) * 40;
                }
                else
                    player1.where.y = goy;
            }
            else
            {
                if ((int)floor(goy / 40) == (int)floor(player1.where.y / 40))
                    player1.where.y = goy;
                else if ((IsThereEmpty(floor(player1.where.x / 40), floor(goy / 40)) == false) || (IsThereEmpty(floor(player1.where.x / 40) + 1, floor(goy / 40)) == false))
                {
                    player1.where.y = goy;
                    player1.where.y = (floor(player1.where.y / 40) + 1) * 40;
                }
                else
                    player1.where.y = goy;
            }
        }
        if (player1.where.y < 0)
            player1.where.y = 0;
    }
    else if ((player1key.right.data & 0x8000) && player1key.direction == 3)
    {
        player1.sight = 2;
        player1.walkingTime += g_fDeltatime;
        if (fmod(player1.where.x, 40) == 0)   //오른쪽으로 움직일때 좌우가 딱 가운데일때
        {
            if (fmod(player1.where.y, 40) == 0)
            {
                if (IsThereEmpty(floor(player1.where.x / 40) + 1, floor(player1.where.y / 40)))
                    player1.where.x += speed;
            }
            else
            {
                if (IsThereEmpty(floor(player1.where.x / 40) + 1, floor(player1.where.y / 40)) && IsThereEmpty(floor(player1.where.x / 40) + 1, floor(player1.where.y / 40) + 1))
                {
                    player1.where.x += speed;
                }
                else if ((IsThereEmpty(floor(player1.where.x / 40) + 1, floor(player1.where.y / 40)) == false) && IsThereEmpty(floor(player1.where.x / 40) + 1, floor(player1.where.y / 40) + 1))
                {
                    player1.where.y += speed;
                    if (player1.position.y < floor(player1.where.y / 40))
                        player1.where.y = floor(player1.where.y / 40) * 40;
                }
                else if (IsThereEmpty(floor(player1.where.x / 40) + 1, floor(player1.where.y / 40)) && (IsThereEmpty(floor(player1.where.x / 40) + 1, floor(player1.where.y / 40) + 1) == false))
                {
                    player1.where.y -= speed;
                    if (floor(player1.where.y / 40) < player1.position.y)
                        player1.where.y = (floor(player1.where.y / 40) + 1) * 40;
                }
            }
        }
        else
        {
            gox = player1.where.x + speed;
            if (fmod(player1.where.y, 40) == 0)
            {
                if ((int)floor(gox / 40) == (int)floor(player1.where.x / 40))
                {
                    player1.where.x = gox;
                }
                else if (IsThereEmpty(floor(gox / 40) + 1, floor(player1.where.y / 40)) == false)
                {
                    player1.where.x = gox;
                    player1.where.x = (floor(player1.where.x / 40)) * 40;
                }
                else
                    player1.where.x = gox;
            }
            else
            {
                if ((int)floor(gox / 40) == (int)floor(player1.where.x / 40))
                {
                    player1.where.x = gox;
                }
                else if ((IsThereEmpty(floor(gox / 40) + 1, floor(player1.where.y / 40)) == false) || (IsThereEmpty(floor(gox / 40) + 1, floor(player1.where.y / 40) + 1) == false))
                {
                    player1.where.x = gox;
                    player1.where.x = (floor(player1.where.x / 40)) * 40;
                }
                else
                    player1.where.x = gox;
            }
        }
        if (player1.where.x > (INGAMEWIDTHSIZE - 40))
            player1.where.x = (INGAMEWIDTHSIZE - 40);
    }
    else if ((player1key.down.data & 0x8000) && player1key.direction == 4)
    {
        player1.sight = 3;
        player1.walkingTime += g_fDeltatime;
        if (fmod(player1.where.y, 40) == 0)   //아래로 움직일때 상하가 딱 가운데일때
        {
            if (fmod(player1.where.x, 40) == 0)
            {
                if (IsThereEmpty(floor(player1.where.x / 40), floor(player1.where.y / 40) + 1))
                    player1.where.y += speed;
            }
            else
            {
                if (IsThereEmpty(floor(player1.where.x / 40), floor(player1.where.y / 40) + 1) && IsThereEmpty(floor(player1.where.x / 40) + 1, floor(player1.where.y / 40) + 1))
                {
                    player1.where.y += speed;
                }
                else if ((IsThereEmpty(floor(player1.where.x / 40), floor(player1.where.y / 40) + 1) == false) && IsThereEmpty(floor(player1.where.x / 40) + 1, floor(player1.where.y / 40) + 1))
                {
                    player1.where.x += speed;
                    if (player1.position.x < floor(player1.where.x / 40))
                        player1.where.x = floor(player1.where.x / 40) * 40;
                }
                else if (IsThereEmpty(floor(player1.where.x / 40), floor(player1.where.y / 40) + 1) && (IsThereEmpty(floor(player1.where.x / 40) + 1, floor(player1.where.y / 40) + 1) == false))
                {
                    player1.where.x -= speed;
                    if (floor(player1.where.x / 40) < player1.position.x)
                        player1.where.x = (floor(player1.where.x / 40) + 1) * 40;
                }
            }
        }
        else
        {
            goy = player1.where.y + speed;
            if (fmod(player1.where.x, 40) == 0)
            {
                if ((int)floor(goy / 40) == (int)floor(player1.where.y / 40))
                    player1.where.y = goy;
                else if (IsThereEmpty(floor(player1.where.x / 40), floor(goy / 40) + 1) == false)
                {
                    player1.where.y = goy;
                    player1.where.y = (floor(player1.where.y / 40)) * 40;
                }
                else
                    player1.where.y = goy;
            }
            else
            {
                if ((int)floor(goy / 40) == (int)floor(player1.where.y / 40))
                    player1.where.y = goy;
                else if ((IsThereEmpty(floor(player1.where.x / 40), floor(goy / 40) + 1) == false) || (IsThereEmpty(floor(player1.where.x / 40) + 1, floor(goy / 40) + 1) == false))
                {
                    player1.where.y = goy;
                    player1.where.y = (floor(player1.where.y / 40)) * 40;
                }
                else
                    player1.where.y = goy;
            }
        }
        if (player1.where.y > (INGAMEHEIGHTSIZE - 40))
            player1.where.y = (INGAMEHEIGHTSIZE - 40);
    }
    else
    {
        player1.walkingTime = 0;
    }

    player1.position.x = (LONG)floor(player1.where.x / 40);
    player1.position.y = (LONG)floor(player1.where.y / 40);
}

void mainCharacterMovePlayer2()
{
    if (player2key.left.data == 0xffff8001)
        player2key.direction = 1;
    if (player2key.up.data == 0xffff8001)
        player2key.direction = 2;
    if (player2key.right.data == 0xffff8001)
        player2key.direction = 3;
    if (player2key.down.data == 0xffff8001)
        player2key.direction = 4;

    float speed = player2.speed * g_fDeltatime;
    float gox, goy;
    if ((player2key.left.data & 0x8000) && player2key.direction == 1)
    {
        player2.sight = 0;
        player2.walkingTime += g_fDeltatime;  //시간에 따라 움직이는 모양 바꾸기.
        if (fmod(player2.where.x, 40) == 0)   //왼쪽으로 움찍일때 좌우가 딱 가운데일때
        {
            if (fmod(player2.where.y, 40) == 0)   //상하도 가운데라면 왼쪽칸만 빈공간이면 움직임.
            {
                if (IsThereEmpty(floor(player2.where.x / 40) - 1, floor(player2.where.y / 40)))
                    player2.where.x -= speed;
            }
            else    //상하가 가운데가 아니라면 
            {
                if (IsThereEmpty(floor(player2.where.x / 40) - 1, floor(player2.where.y / 40)) && IsThereEmpty(floor(player2.where.x / 40) - 1, floor(player2.where.y / 40) + 1))
                {   //왼쪽 위와 아래가 둘다 뚫렸다면 움직임
                    player2.where.x -= speed;
                }
                else if ((IsThereEmpty(floor(player2.where.x / 40) - 1, floor(player2.where.y / 40)) == false) && IsThereEmpty(floor(player2.where.x / 40) - 1, floor(player2.where.y / 40) + 1))
                {   //왼쪽위는 막히고 왼쪽아래가 뚫렸을때 아래로움직임
                    player2.where.y += speed;
                    if (player2.position.y < floor(player2.where.y / 40))
                        player2.where.y = floor(player2.where.y / 40) * 40;
                }
                else if (IsThereEmpty(floor(player2.where.x / 40) - 1, floor(player2.where.y / 40)) && (IsThereEmpty(floor(player2.where.x / 40) - 1, floor(player2.where.y / 40) + 1) == false))
                {   //왼쪽아래가 막혔을때 왼쪽위로움직임
                    player2.where.y -= speed;
                    if (floor(player2.where.y / 40) < player2.position.y)
                        player2.where.y = (floor(player2.where.y / 40) + 1) * 40;
                }
            }
        }
        else
        {
            gox = player2.where.x - speed;
            if (fmod(player2.where.y, 40) == 0)   //상하가 가운데일때
            {
                if ((int)floor(gox / 40) == (int)floor(player2.where.x / 40)) //현재 캐릭터칸과 움직일 칸이 같으면 움직임
                {
                    player2.where.x = gox;
                }
                else if (IsThereEmpty(floor(gox / 40), floor(player2.where.y / 40)) == false) //현재 캐릭터칸과 움직일 칸이 다르고 빈공간이 아니라면 벽에 막히기
                {
                    player2.where.x = gox;
                    player2.where.x = (floor(player2.where.x / 40) + 1) * 40;
                }
                else    //현재 캐릭터칸과 움직일 칸이 다르고 빈공간이라면 움직이기
                    player2.where.x = gox;
            }
            else
            {
                if ((int)floor(gox / 40) == (int)floor(player2.where.x / 40)) //현재 캐릭터칸과 움직일 칸이 같으면 움직임
                {
                    player2.where.x = gox;
                }
                else if ((IsThereEmpty(floor(gox / 40), floor(player2.where.y / 40)) == false) || (IsThereEmpty(floor(gox / 40), floor(player2.where.y / 40) + 1) == false)) //현재 캐릭터칸과 움직일 칸이 다르고 빈공간이 아니라면 벽에 막히기
                {
                    player2.where.x = gox;
                    player2.where.x = (floor(player2.where.x / 40) + 1) * 40;
                }
                else    //현재 캐릭터칸과 움직일 칸이 다르고 빈공간이라면 움직이기
                    player2.where.x = gox;
            }
        }
        if (player2.where.x < 0)
            player2.where.x = 0;
    }
    else if ((player2key.up.data & 0x8000) && player2key.direction == 2)
    {
        player2.sight = 1;
        player2.walkingTime += g_fDeltatime;
        if (fmod(player2.where.y, 40) == 0)   //위로 움직일때 상하가 딱 가운데일때
        {
            if (fmod(player2.where.x, 40) == 0)
            {
                if (IsThereEmpty(floor(player2.where.x / 40), floor(player2.where.y / 40) - 1))
                    player2.where.y -= speed;
            }
            else
            {
                if (IsThereEmpty(floor(player2.where.x / 40), floor(player2.where.y / 40) - 1) && IsThereEmpty(floor(player2.where.x / 40) + 1, floor(player2.where.y / 40) - 1))
                {
                    player2.where.y -= speed;
                }
                else if ((IsThereEmpty(floor(player2.where.x / 40), floor(player2.where.y / 40) - 1) == false) && IsThereEmpty(floor(player2.where.x / 40) + 1, floor(player2.where.y / 40) - 1))
                {
                    player2.where.x += speed;
                    if (player2.position.x < floor(player2.where.x / 40))
                        player2.where.x = floor(player2.where.x / 40) * 40;
                }
                else if (IsThereEmpty(floor(player2.where.x / 40), floor(player2.where.y / 40) - 1) && (IsThereEmpty(floor(player2.where.x / 40) + 1, floor(player2.where.y / 40) - 1) == false))
                {
                    player2.where.x -= speed;
                    if (floor(player2.where.x / 40) < player2.position.x)
                        player2.where.x = (floor(player2.where.x / 40) + 1) * 40;
                }
            }
        }
        else
        {
            goy = player2.where.y - speed;
            if (fmod(player2.where.x, 40) == 0)
            {
                if ((int)floor(goy / 40) == (int)floor(player2.where.y / 40))
                    player2.where.y = goy;
                else if (IsThereEmpty(floor(player2.where.x / 40), floor(goy / 40)) == false)
                {
                    player2.where.y = goy;
                    player2.where.y = (floor(player2.where.y / 40) + 1) * 40;
                }
                else
                    player2.where.y = goy;
            }
            else
            {
                if ((int)floor(goy / 40) == (int)floor(player2.where.y / 40))
                    player2.where.y = goy;
                else if ((IsThereEmpty(floor(player2.where.x / 40), floor(goy / 40)) == false) || (IsThereEmpty(floor(player2.where.x / 40) + 1, floor(goy / 40)) == false))
                {
                    player2.where.y = goy;
                    player2.where.y = (floor(player2.where.y / 40) + 1) * 40;
                }
                else
                    player2.where.y = goy;
            }
        }
        if (player2.where.y < 0)
            player2.where.y = 0;
    }
    else if ((player2key.right.data & 0x8000) && player2key.direction == 3)
    {
        player2.sight = 2;
        player2.walkingTime += g_fDeltatime;
        if (fmod(player2.where.x, 40) == 0)   //오른쪽으로 움직일때 좌우가 딱 가운데일때
        {
            if (fmod(player2.where.y, 40) == 0)
            {
                if (IsThereEmpty(floor(player2.where.x / 40) + 1, floor(player2.where.y / 40)))
                    player2.where.x += speed;
            }
            else
            {
                if (IsThereEmpty(floor(player2.where.x / 40) + 1, floor(player2.where.y / 40)) && IsThereEmpty(floor(player2.where.x / 40) + 1, floor(player2.where.y / 40) + 1))
                {
                    player2.where.x += speed;
                }
                else if ((IsThereEmpty(floor(player2.where.x / 40) + 1, floor(player2.where.y / 40)) == false) && IsThereEmpty(floor(player2.where.x / 40) + 1, floor(player2.where.y / 40) + 1))
                {
                    player2.where.y += speed;
                    if (player2.position.y < floor(player2.where.y / 40))
                        player2.where.y = floor(player2.where.y / 40) * 40;
                }
                else if (IsThereEmpty(floor(player2.where.x / 40) + 1, floor(player2.where.y / 40)) && (IsThereEmpty(floor(player2.where.x / 40) + 1, floor(player2.where.y / 40) + 1) == false))
                {
                    player2.where.y -= speed;
                    if (floor(player2.where.y / 40) < player2.position.y)
                        player2.where.y = (floor(player2.where.y / 40) + 1) * 40;
                }
            }
        }
        else
        {
            gox = player2.where.x + speed;
            if (fmod(player2.where.y, 40) == 0)
            {
                if ((int)floor(gox / 40) == (int)floor(player2.where.x / 40))
                {
                    player2.where.x = gox;
                }
                else if (IsThereEmpty(floor(gox / 40) + 1, floor(player2.where.y / 40)) == false)
                {
                    player2.where.x = gox;
                    player2.where.x = (floor(player2.where.x / 40)) * 40;
                }
                else
                    player2.where.x = gox;
            }
            else
            {
                if ((int)floor(gox / 40) == (int)floor(player2.where.x / 40))
                {
                    player2.where.x = gox;
                }
                else if ((IsThereEmpty(floor(gox / 40) + 1, floor(player2.where.y / 40)) == false) || (IsThereEmpty(floor(gox / 40) + 1, floor(player2.where.y / 40) + 1) == false))
                {
                    player2.where.x = gox;
                    player2.where.x = (floor(player2.where.x / 40)) * 40;
                }
                else
                    player2.where.x = gox;
            }
        }
        if (player2.where.x > (INGAMEWIDTHSIZE - 40))
            player2.where.x = (INGAMEWIDTHSIZE - 40);
    }
    else if ((player2key.down.data & 0x8000) && player2key.direction == 4)
    {
        player2.sight = 3;
        player2.walkingTime += g_fDeltatime;
        if (fmod(player2.where.y, 40) == 0)   //아래로 움직일때 상하가 딱 가운데일때
        {
            if (fmod(player2.where.x, 40) == 0)
            {
                if (IsThereEmpty(floor(player2.where.x / 40), floor(player2.where.y / 40) + 1))
                    player2.where.y += speed;
            }
            else
            {
                if (IsThereEmpty(floor(player2.where.x / 40), floor(player2.where.y / 40) + 1) && IsThereEmpty(floor(player2.where.x / 40) + 1, floor(player2.where.y / 40) + 1))
                {
                    player2.where.y += speed;
                }
                else if ((IsThereEmpty(floor(player2.where.x / 40), floor(player2.where.y / 40) + 1) == false) && IsThereEmpty(floor(player2.where.x / 40) + 1, floor(player2.where.y / 40) + 1))
                {
                    player2.where.x += speed;
                    if (player2.position.x < floor(player2.where.x / 40))
                        player2.where.x = floor(player2.where.x / 40) * 40;
                }
                else if (IsThereEmpty(floor(player2.where.x / 40), floor(player2.where.y / 40) + 1) && (IsThereEmpty(floor(player2.where.x / 40) + 1, floor(player2.where.y / 40) + 1) == false))
                {
                    player2.where.x -= speed;
                    if (floor(player2.where.x / 40) < player2.position.x)
                        player2.where.x = (floor(player2.where.x / 40) + 1) * 40;
                }
            }
        }
        else
        {
            goy = player2.where.y + speed;
            if (fmod(player2.where.x, 40) == 0)
            {
                if ((int)floor(goy / 40) == (int)floor(player2.where.y / 40))
                    player2.where.y = goy;
                else if (IsThereEmpty(floor(player2.where.x / 40), floor(goy / 40) + 1) == false)
                {
                    player2.where.y = goy;
                    player2.where.y = (floor(player2.where.y / 40)) * 40;
                }
                else
                    player2.where.y = goy;
            }
            else
            {
                if ((int)floor(goy / 40) == (int)floor(player2.where.y / 40))
                    player2.where.y = goy;
                else if ((IsThereEmpty(floor(player2.where.x / 40), floor(goy / 40) + 1) == false) || (IsThereEmpty(floor(player2.where.x / 40) + 1, floor(goy / 40) + 1) == false))
                {
                    player2.where.y = goy;
                    player2.where.y = (floor(player2.where.y / 40)) * 40;
                }
                else
                    player2.where.y = goy;
            }
        }
        if (player2.where.y > (INGAMEHEIGHTSIZE - 40))
            player2.where.y = (INGAMEHEIGHTSIZE - 40);
    }
    else
    {
        player2.walkingTime = 0;
    }

    player2.position.x = (LONG)floor(player2.where.x / 40);
    player2.position.y = (LONG)floor(player2.where.y / 40);
}

void pressBalloonPlayer1(int vKey)
{
    if (GetAsyncKeyState(vKey) & 0x8000)
    {
        int x = (int)floor((player1.where.x + 20) / 40);
        int y = (int)floor((player1.where.y + 20) / 40);
        if (canSetWaterBalloonPlayer1(x, y))
            setWaterBalloonPlayer1(x, y);
    }
}

void pressBalloonPlayer2(int vKey)
{
    if (GetAsyncKeyState(vKey) & 0x8000)
    {
        int x = (int)floor((player2.where.x + 20) / 40);
        int y = (int)floor((player2.where.y + 20) / 40);
        if (canSetWaterBalloonPlayer2(x, y))
            setWaterBalloonPlayer2(x, y);
    }
}

bool canSetWaterBalloonPlayer1(int x, int y)
{
    if ((x < 0) || (INGAMEWIDTHSIZE / 40 <= x) || (y < 0) || (INGAMEHEIGHTSIZE / 40 <= y))
        return false;
    if (hardBlock->info[y][x] != 0)
        return false;
    if (softBlock->info[y][x] != 0)
        return false;
    if (waterBalloonBlock->info[y][x].what != 0)
        return false;
    //모든 블럭이 다 빈곳일 때만 물풍선을 놓을 수 있음
    if (player1.inWaterBalloon == true)
        return false;
    //물풍선 안에 갇혀 있을 때도 물풍선은 놓을 수 없음
    if (player1.waterballoonMaxNum <= player1.waterballoonNum)
        return false;
    //최대 물풍선 개수를 넘길 수 없음.
    return true;
}

bool canSetWaterBalloonPlayer2(int x, int y)
{
    if ((x < 0) || (INGAMEWIDTHSIZE / 40 <= x) || (y < 0) || (INGAMEHEIGHTSIZE / 40 <= y))
        return false;
    if (hardBlock->info[y][x] != 0)
        return false;
    if (softBlock->info[y][x] != 0)
        return false;
    if (waterBalloonBlock->info[y][x].what != 0)
        return false;
    //모든 블럭이 다 빈곳일 때만 물풍선을 놓을 수 있음
    if (player2.inWaterBalloon == true)
        return false;
    //물풍선 안에 갇혀 있을 때도 물풍선은 놓을 수 없음
    if (player2.waterballoonMaxNum <= player2.waterballoonNum)
        return false;
    //최대 물풍선 개수를 넘길 수 없음.
    return true;
}

void setWaterBalloonPlayer1(int x, int y)
{
    PlaySound(TEXT("sound\\물풍선.wav"), NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
    player1.waterballoonCount++;
    if (100 <= player1.waterballoonCount)
        player1.waterballoonCount = 0;

    waterBalloonBlock->info[y][x].info->length = player1.waterballoonSize;
    waterBalloonBlock->info[y][x].info->pop = false;
    //waterBalloonBlock->info[y][x].info->where.x = x;
    //waterBalloonBlock->info[y][x].info->where.y = y;
    waterBalloonBlock->info[y][x].info->waitingtTime = 0.0;
    waterBalloonBlock->info[y][x].info->boomTime = 0.0;
    waterBalloonBlock->info[y][x].what = 10;
    waterBalloonBlock->info[y][x].player = 1;

    player1.waterballoonNum++;
    //현재 물풍선 개수가 올라감.
}

void setWaterBalloonPlayer2(int x, int y)
{
    PlaySound(TEXT("sound\\물풍선.wav"), NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
    player2.waterballoonCount++;
    if (100 <= player2.waterballoonCount)
        player2.waterballoonCount = 0;

    waterBalloonBlock->info[y][x].info->length = player2.waterballoonSize;
    waterBalloonBlock->info[y][x].info->pop = false;
    //waterBalloonBlock->info[y][x].info->where.x = x;
    //waterBalloonBlock->info[y][x].info->where.y = y;
    waterBalloonBlock->info[y][x].info->waitingtTime = 0.0;
    waterBalloonBlock->info[y][x].info->boomTime = 0.0;
    waterBalloonBlock->info[y][x].what = 10;
    waterBalloonBlock->info[y][x].player = 2;

    player2.waterballoonNum++;
    //현재 물풍선 개수가 올라감.
}

void getItemPlayer1()
{
    int x = (int)floor((player1.where.x + 20) / 40);
    int y = (int)floor((player1.where.y + 20) / 40);
    int iteminfo = itemBlock->info[y][x];
    if (iteminfo != 0)
    {
        if (iteminfo == 1)
            player1.speed += SPEEDADD;
        else if (iteminfo == 2)
            player1.waterballoonSize += 1;
        else if (iteminfo == 3)
            player1.waterballoonMaxNum += 1;

        itemBlock->info[y][x] = 0;
        mciSendString(TEXT("play sound\\아이템.wav"), NULL, 0, NULL);
    }
}

void getItemPlayer2()
{
    int x = (int)floor((player2.where.x + 20) / 40);
    int y = (int)floor((player2.where.y + 20) / 40);
    int iteminfo = itemBlock->info[y][x];
    if (iteminfo != 0)
    {
        if (iteminfo == 1)
            player2.speed += SPEEDADD;
        else if (iteminfo == 2)
            player2.waterballoonSize += 1;
        else if (iteminfo == 3)
            player2.waterballoonMaxNum += 1;

        itemBlock->info[y][x] = 0;
        mciSendString(TEXT("play sound\\아이템.wav"), NULL, 0, NULL);
    }
}

bool IsThereEmpty(int x, int y)
{
    if ((x < 0) || (INGAMEWIDTHSIZE / 40 <= x) || (y < 0) || (INGAMEHEIGHTSIZE / 40 <= y))
        return false;
    if (hardBlock->info[y][x] != 0)
        return false;
    if (softBlock->info[y][x] != 0)
        return false;
    if (waterBalloonBlock->info[y][x].what == 10)
        //물풍선은 10일때만 칸을 차지하고 
        return false;
    return true;
}

void balloonTimeAndPop()    //0빈칸 10물풍선 11왼쪽물줄기 12위물줄기 13오른쪽물줄기 14아래물줄기 15중간터지는거
{
    int leftB = 11;
    int upB = 12;
    int rightB = 13;
    int downB = 14;
    int midB = 15;
    for (int x = 0; x < MAPWIDTH; x++)
    {
        for (int y = 0; y < MAPHEIGHT; y++)
        {
            if (waterBalloonBlock->info[y][x].what == 10) //물풍선이라면 물줄기 길이에 따라서 상하좌우 물줄기줄기
            {
                if (waterBalloonBlock->info[y][x].info->pop == false)
                {   //물풍선이 터지지 않은 상태라면 기다리는시간이 점점 늘어남.
                    waterBalloonBlock->info[y][x].info->waitingtTime += g_fDeltatime;
                    if (2.0 < waterBalloonBlock->info[y][x].info->waitingtTime)
                        balloonPop(x, y);
                }
            }
            else if (10 < waterBalloonBlock->info[y][x].what) //물줄기상태라면
            {
                waterBalloonBlock->info[y][x].info->boomTime += g_fDeltatime;
                if (POPTIME < waterBalloonBlock->info[y][x].info->boomTime)
                {
                    waterBalloonBlock->info[y][x].what = 0;
                    //터지고 POPTIME 초 지나면 사라짐.
                }
            }
        }
    }
}

void balloonPop(int x, int y)
{
    waterBalloonBlock->info[y][x].info->pop = true;
    waterBalloonBlock->info[y][x].info->boomTime = 0.0;
    waterBalloonBlock->info[y][x].what = 15;
    //물줄기 길이만큼 상하좌우가 물줄기로 바뀜.
    if(waterBalloonBlock->info[y][x].player==1)
        player1.waterballoonNum -= 1;
    else if (waterBalloonBlock->info[y][x].player == 2)
        player2.waterballoonNum -= 1;
    //다 터지고나면 물풍선 놓을 수 있음.
    mciSendString(TEXT("play sound\\물풍선펑.wav"), NULL, 0, NULL);
    for (int i = 1; i <= waterBalloonBlock->info[y][x].info->length; i++)
    {
        //맵밖으로 나가거나 hardBlock을 만나면 반복문 종료
        if (((x - i) < 0) || (hardBlock->info[y][x - i] != 0))
            break;
        //softBlock만나면 블록터뜨리기
        else if (softBlock->info[y][x - i] != 0)
        {
            softBlock->pop[y][x - i] = 1;
            softBlock->poptime[y][x] = 0.0;
            break;
        }
        //물풍선만나면 터뜨려버려!
        else if (waterBalloonBlock->info[y][x - i].what == 10)
            balloonPop(x - i, y);
        else
        {
            waterBalloonBlock->info[y][x - i].what = 11;
            waterBalloonBlock->info[y][x - i].info->boomTime = 0.0;

            if (itemBlock->info[y][x - i] != 0)
                itemBlock->info[y][x - i] = 0;
        }
    }
    //왼쪽
    for (int i = 1; i <= waterBalloonBlock->info[y][x].info->length; i++)
    {
        //맵밖으로 나가거나 hardBlock을 만나면 반복문 종료
        if (((y - i) < 0) || (hardBlock->info[y - i][x] != 0))
            break;
        //softBlock만나면 블록터뜨리기
        else if (softBlock->info[y - i][x] != 0)
        {
            softBlock->pop[y - i][x] = 1;
            softBlock->poptime[y][x] = 0.0;
            break;
        }
        //물풍선만나면 터뜨려버려!
        else if (waterBalloonBlock->info[y - i][x].what == 10)
            balloonPop(x, y - i);
        else
        {
            waterBalloonBlock->info[y - i][x].what = 12;
            waterBalloonBlock->info[y - i][x].info->boomTime = 0.0;

            if (itemBlock->info[y - i][x] != 0)
                itemBlock->info[y - i][x] = 0;
        }
    }
    //위쪽
    for (int i = 1; i <= waterBalloonBlock->info[y][x].info->length; i++)
    {
        //맵밖으로 나가거나 hardBlock을 만나면 반복문 종료
        if (((x + i) >= (INGAMEWIDTHSIZE / 40)) || (hardBlock->info[y][x + i] != 0))
            break;
        //softBlock만나면 블록터뜨리기
        else if (softBlock->info[y][x + i] != 0)
        {
            softBlock->pop[y][x + i] = 1;
            softBlock->poptime[y][x] = 0.0;
            break;
        }
        //물풍선만나면 터뜨려버려!
        else if (waterBalloonBlock->info[y][x + i].what == 10)
            balloonPop(x + i, y);
        else
        {
            waterBalloonBlock->info[y][x + i].what = 13;
            waterBalloonBlock->info[y][x + i].info->boomTime = 0.0;

            if (itemBlock->info[y][x + i] != 0)
                itemBlock->info[y][x + i] = 0;
        }
    }
    //오른쪽
    for (int i = 1; i <= waterBalloonBlock->info[y][x].info->length; i++)
    {
        //맵밖으로 나가거나 hardBlock을 만나면 반복문 종료
        if (((y + i) >= (INGAMEHEIGHTSIZE / 40)) || (hardBlock->info[y + i][x] != 0))
            break;
        //softBlock만나면 블록터뜨리기
        else if (softBlock->info[y + i][x] != 0)
        {
            softBlock->pop[y + i][x] = 1;
            softBlock->poptime[y][x] = 0.0;
            break;
        }
        //물풍선만나면 터뜨려버려!
        else if (waterBalloonBlock->info[y + i][x].what == 10)
            balloonPop(x, y + i);
        else
        {
            waterBalloonBlock->info[y + i][x].what = 14;
            waterBalloonBlock->info[y + i][x].info->boomTime = 0.0;

            if (itemBlock->info[y + i][x] != 0)
                itemBlock->info[y + i][x] = 0;
        }
    }
    //아래쪽
}

void softBlockPop()
{
    for (int x = 0; x < MAPWIDTH; x++)
    {
        for (int y = 0; y < MAPHEIGHT; y++)
        {
            if (softBlock->pop[y][x] == 1)
            {
                softBlock->poptime[y][x] += g_fDeltatime;
                if (POPTIME < softBlock->poptime[y][x])
                {
                    softBlock->info[y][x] = 0;
                    softBlock->pop[y][x] = 0;
                    itemBlock->info[y][x] = softBlock->item[y][x];
                }
            }
        }
    }
}

void itemTimeflow()
{
    for (int x = 0; x < MAPWIDTH; x++)
    {
        for (int y = 0; y < MAPHEIGHT; y++)
        {
            if (itemBlock->info[y][x] != 0)
                itemBlock->time[y][x] += g_fDeltatime;
        }
    }
}

void touchWaterBalloonPlayer1()
{
    int x = (int)floor((player1.where.x + 20) / 40);
    int y = (int)floor((player1.where.y + 20) / 40);
    if ((11 <= waterBalloonBlock->info[y][x].what) && (waterBalloonBlock->info[y][x].what <= 15))
        player1.inWaterBalloon = true;
}

void touchWaterBalloonPlayer2()
{
    int x = (int)floor((player2.where.x + 20) / 40);
    int y = (int)floor((player2.where.y + 20) / 40);
    if ((11 <= waterBalloonBlock->info[y][x].what) && (waterBalloonBlock->info[y][x].what <= 15))
        player2.inWaterBalloon = true;
}

void inWaterBalloonDoPlayer1()
{
    if (player1.inWaterBalloon == true)
    {
        player1.inWaterBalloonTime += g_fDeltatime;
        if (3.0 <= player1.inWaterBalloonTime)
            player1.dead = true;

        if (player2.inWaterBalloon == false)
        {
            int x = (int)floor((player1.where.x + 20) / 40);
            int y = (int)floor((player1.where.y + 20) / 40);

            int x2 = (int)floor((player2.where.x + 20) / 40);
            int y2 = (int)floor((player2.where.y + 20) / 40);

            if ((x == x2) && (y == y2))
                player1.dead = true;
        }
        //물풍선에 갇혀있을 때 상대방에 닿는다면 실패.
    }
}

void inWaterBalloonDoPlayer2()
{
    if (player2.inWaterBalloon == true)
    {
        player2.inWaterBalloonTime += g_fDeltatime;
        if (3.0 <= player2.inWaterBalloonTime)
            player2.dead = true;

        if (player1.inWaterBalloon == false)
        {
            int x = (int)floor((player1.where.x + 20) / 40);
            int y = (int)floor((player1.where.y + 20) / 40);

            int x2 = (int)floor((player2.where.x + 20) / 40);
            int y2 = (int)floor((player2.where.y + 20) / 40);

            if ((x == x2) && (y == y2))
                player2.dead = true;
        }
        //물풍선에 갇혀있을 때 상대방에 닿는다면 실패.
    }
}

void whoIsWin()
{
    if ((player1.dead == true) && (player2.dead == false))
        background.winner = 1;
    else if ((player1.dead == false) && (player2.dead == true))
        background.winner = 2;
    else if ((player1.dead == true) && (player2.dead == true))
        background.winner = 3;
    if (background.winner != 0)
    {
        background.closeTime += g_fDeltatime;
        if (3.0 < background.closeTime)
            closeInGame();
    }
}

void closeInGame()
{
    background.ready = 1;
    free(hardBlock);
    free(softBlock);
    free(itemBlock);
    for (int x = 0; x < INGAMEWIDTHSIZE / 40; x++)
    {
        for (int y = 0; y < INGAMEHEIGHTSIZE / 40; y++)
        {
            free(waterBalloonBlock->info[y][x].info);
        }
    }
    free(waterBalloonBlock);

    PlaySound(NULL, 0, 0);
    PlaySound(TEXT("sound\\lobby.wav"), NULL, SND_FILENAME | SND_ASYNC | SND_LOOP);
}

void sendReady()
{
    char message[30] = "this is message";
    sprintf(message, "0 1"); //맨 앞이 0이면 게임시작 신호.
    send(hSock, message, sizeof(message), 0);
}

void drawBackground(Graphics* g)
{
    if (background.ready == 1)
    {
        g->DrawImage(background.readyImage, 0, 0, CLIENTWIDTHSIZE, CLIENTHEIGHTSIZE);
        if ((516 <= ptMouse.x) && (498 <= ptMouse.y) && (ptMouse.x <= 700) && (ptMouse.y <= 546))
        {
            if ((int)((background.readyMouseTime) / 0.1) % 2 == 0)
                g->DrawImage(background.ready1, 513, 495);
            else
                g->DrawImage(background.ready2, 513, 495);
        }
    }
    else
    {
        g->DrawImage(background.inGameImage, 0, 0, CLIENTWIDTHSIZE, CLIENTHEIGHTSIZE);
        g->DrawImage(background.map1Image, WIDTHGAP, HEIGHTGAP, INGAMEWIDTHSIZE, INGAMEHEIGHTSIZE);
    }
}

void drawAll(Graphics* g)
{
    //딱 40x40으로 그릴게 아니라, 키큰건 40픽셀을 넘게 그려야 하므로, 
    //윗줄부터 아랫줄까지 순서대로 그려야됨.
    int charY1 = (player1.where.y + 19) / 40;
    int charY2 = (player2.where.y + 19) / 40;
    int x, y;
    for (y = 0; y < MAPHEIGHT; y++)
    {
        for (x = 0; x < MAPWIDTH; x++)
        {
            if (softBlock->info[y][x] == 100)
            {
                if (softBlock->pop[y][x] == 0)
                    g->DrawImage(softBlockImage.redBlock, WIDTHGAP + x * 40, HEIGHTGAP + y * 40 - 4);
                else if ((int)(softBlock->poptime[y][x] * 25) % 2)
                    g->DrawImage(softBlockImage.redBlockPop1, WIDTHGAP + x * 40, HEIGHTGAP + y * 40 - 4, 40, 44);
                else
                    g->DrawImage(softBlockImage.redBlockPop2, WIDTHGAP + x * 40, HEIGHTGAP + y * 40 - 4, 40, 44);
            }
            else if (softBlock->info[y][x] == 110)
            {
                if (softBlock->pop[y][x] == 0)
                    g->DrawImage(softBlockImage.orangeBlock, WIDTHGAP + x * 40, HEIGHTGAP + y * 40 - 4);
                else if ((int)(softBlock->poptime[y][x] * 25) % 2)
                    g->DrawImage(softBlockImage.orangeBlockPop1, WIDTHGAP + x * 40, HEIGHTGAP + y * 40 - 4, 40, 44);
                else
                    g->DrawImage(softBlockImage.orangeBlockPop2, WIDTHGAP + x * 40, HEIGHTGAP + y * 40 - 4, 40, 44);
            }
            else if (softBlock->info[y][x] == 120)
            {
                if (softBlock->pop[y][x] == 0)
                    g->DrawImage(softBlockImage.brownBlock, WIDTHGAP + x * 40, HEIGHTGAP + y * 40 - 4);
                else if ((int)(softBlock->poptime[y][x] * 25) % 2)
                    g->DrawImage(softBlockImage.brownBlockPop1, WIDTHGAP + x * 40, HEIGHTGAP + y * 40 - 4, 40, 44);
                else
                    g->DrawImage(softBlockImage.brownBlockPop2, WIDTHGAP + x * 40, HEIGHTGAP + y * 40 - 4, 40, 44);

            }

            else if (hardBlock->info[y][x] == 100)
                g->DrawImage(hardBlockImage.house1, WIDTHGAP + x * 40, HEIGHTGAP + y * 40 - 17);
            else if (hardBlock->info[y][x] == 110)
                g->DrawImage(hardBlockImage.house2, WIDTHGAP + x * 40, HEIGHTGAP + y * 40 - 17);
            else if (hardBlock->info[y][x] == 120)
                g->DrawImage(hardBlockImage.tree, WIDTHGAP + x * 40, HEIGHTGAP + y * 40 - 30, 40, 70);

            else if (itemBlock->info[y][x] == 1)
            {
                if ((int)(itemBlock->time[y][x] * 25) % 2)
                    g->DrawImage(itemBlockImage.speed1, WIDTHGAP + x * 40, HEIGHTGAP + y * 40 - 23, 40, 63);
                else
                    g->DrawImage(itemBlockImage.speed2, WIDTHGAP + x * 40, HEIGHTGAP + y * 40 - 23, 40, 63);
            }
            else if (itemBlock->info[y][x] == 2)
            {
                if ((int)(itemBlock->time[y][x] * 25) % 2)
                    g->DrawImage(itemBlockImage.waterLength1, WIDTHGAP + x * 40, HEIGHTGAP + y * 40 - 21, 40, 61);
                else
                    g->DrawImage(itemBlockImage.waterLength2, WIDTHGAP + x * 40, HEIGHTGAP + y * 40 - 21, 40, 61);
            }
            else if (itemBlock->info[y][x] == 3)
            {
                if ((int)(itemBlock->time[y][x] * 25) % 2)
                    g->DrawImage(itemBlockImage.waterBalloon1, WIDTHGAP + x * 40, HEIGHTGAP + y * 40 - 24, 40, 64);
                else
                    g->DrawImage(itemBlockImage.waterBalloon2, WIDTHGAP + x * 40, HEIGHTGAP + y * 40 - 24, 40, 64);
            }

            else if (waterBalloonBlock->info[y][x].what == 10)
                g->DrawImage(waterBalloon.image[(int)(waterBalloonBlock->info[y][x].info->waitingtTime * 8) % 8], WIDTHGAP + x * 40 - 27, HEIGHTGAP + y * 40 - 36, 90, 90);
            else if (waterBalloonBlock->info[y][x].what == 11)
                g->DrawImage(waterBalloon.boomImage[(int)(waterBalloonBlock->info[y][x].info->boomTime * 25) % 2], WIDTHGAP + x * 40, HEIGHTGAP + y * 40, 40, 40);
            else if (waterBalloonBlock->info[y][x].what == 12)
                g->DrawImage(waterBalloon.boomImage[(int)(waterBalloonBlock->info[y][x].info->boomTime * 25) % 2 + 2], WIDTHGAP + x * 40, HEIGHTGAP + y * 40, 40, 40);
            else if (waterBalloonBlock->info[y][x].what == 13)
                g->DrawImage(waterBalloon.boomImage[(int)(waterBalloonBlock->info[y][x].info->boomTime * 25) % 2 + 4], WIDTHGAP + x * 40, HEIGHTGAP + y * 40, 40, 40);
            else if (waterBalloonBlock->info[y][x].what == 14)
                g->DrawImage(waterBalloon.boomImage[(int)(waterBalloonBlock->info[y][x].info->boomTime * 25) % 2 + 6], WIDTHGAP + x * 40, HEIGHTGAP + y * 40, 40, 40);
            else if (waterBalloonBlock->info[y][x].what == 15)
                g->DrawImage(waterBalloon.boomImage[(int)(waterBalloonBlock->info[y][x].info->boomTime * 25) % 2 + 8], WIDTHGAP + x * 40, HEIGHTGAP + y * 40, 40, 40);
        }
        if (charY1 == y)
            drawCharacterPlayer1(g);
        if (charY2 == y)
            drawCharacterPlayer2(g);
    }
    //drawInfo();
    if (background.winner != 0)
        g->DrawImage(background.winImage, 400-129, 100, background.winImage->GetWidth(), background.winImage->GetHeight());
}

void drawInfo()
{
    //if (waterBalloonBlock->info[0][0].what == 10)
    wsprintf(info, L"%d %d", playerNum, player1.inWaterBalloon);
    TextOut(backMemDC, 0, 0, info, lstrlen(info));
}

void drawCharacterPlayer1(Graphics* g)
{
    if (background.ready == 1)
    {
    }
    else
    {
        int frame = (int)(player1.walkingTime * 10) % 6;
        int balloonFrame= (int)(player1.inWaterBalloonTime * 10) % 2;
        if (player1.dead == true)
            g->DrawImage(player1.deadImage[balloonFrame], WIDTHGAP + (int)floor(player1.where.x), HEIGHTGAP + (int)floor(player1.where.y) - (player1.deadImage[balloonFrame]->GetHeight() - 40), 40, player1.deadImage[balloonFrame]->GetHeight());
        else if (player1.inWaterBalloon == true)
            g->DrawImage(player1.inWaterBalloonImage[balloonFrame], WIDTHGAP + (int)floor(player1.where.x), HEIGHTGAP + (int)floor(player1.where.y) - (player1.inWaterBalloonImage[balloonFrame]->GetHeight() - 40), 40, player1.inWaterBalloonImage[balloonFrame]->GetHeight());
        else if (player1.sight == 0)
            g->DrawImage(player1.leftImage[frame], WIDTHGAP + (int)floor(player1.where.x), HEIGHTGAP + (int)floor(player1.where.y) - (player1.leftImage[frame]->GetHeight() - 40), 40, player1.leftImage[frame]->GetHeight());
        else if (player1.sight == 1)
            g->DrawImage(player1.upImage[frame], WIDTHGAP + (int)floor(player1.where.x), HEIGHTGAP + (int)floor(player1.where.y) - (player1.upImage[frame]->GetHeight() - 40), 40, player1.upImage[frame]->GetHeight());
        else if (player1.sight == 2)
            g->DrawImage(player1.rightImage[frame], WIDTHGAP + (int)floor(player1.where.x), HEIGHTGAP + (int)floor(player1.where.y) - (player1.rightImage[frame]->GetHeight() - 40), 40, player1.rightImage[frame]->GetHeight());
        else if (player1.sight == 3)
            g->DrawImage(player1.downImage[frame], WIDTHGAP + (int)floor(player1.where.x), HEIGHTGAP + (int)floor(player1.where.y) - (player1.downImage[frame]->GetHeight() - 40), 40, player1.downImage[frame]->GetHeight());
    }
}

void drawCharacterPlayer2(Graphics* g)
{
    if (background.ready == 1)
    {
    }
    else
    {
        int frame2 = (int)(player2.walkingTime * 10) % 6;
        int balloonFrame = (int)(player2.inWaterBalloonTime * 10) % 2;
        if (player2.dead == true)
            g->DrawImage(player1.deadImage[balloonFrame], WIDTHGAP + (int)floor(player2.where.x), HEIGHTGAP + (int)floor(player2.where.y) - (player1.deadImage[balloonFrame]->GetHeight() - 40), 40, player1.deadImage[balloonFrame]->GetHeight());
        else if (player2.inWaterBalloon == true)
            g->DrawImage(player1.inWaterBalloonImage[balloonFrame], WIDTHGAP + (int)floor(player2.where.x), HEIGHTGAP + (int)floor(player2.where.y) - (player1.inWaterBalloonImage[balloonFrame]->GetHeight() - 40), 40, player1.inWaterBalloonImage[balloonFrame]->GetHeight());
        else if (player2.sight == 0)
            g->DrawImage(player1.leftImage[frame2], WIDTHGAP + (int)floor(player2.where.x), HEIGHTGAP + (int)floor(player2.where.y) - (player1.leftImage[frame2]->GetHeight() - 40), 40, player1.leftImage[frame2]->GetHeight());
        else if (player2.sight == 1)
            g->DrawImage(player1.upImage[frame2], WIDTHGAP + (int)floor(player2.where.x), HEIGHTGAP + (int)floor(player2.where.y) - (player1.upImage[frame2]->GetHeight() - 40), 40, player1.upImage[frame2]->GetHeight());
        else if (player2.sight == 2)
            g->DrawImage(player1.rightImage[frame2], WIDTHGAP + (int)floor(player2.where.x), HEIGHTGAP + (int)floor(player2.where.y) - (player1.rightImage[frame2]->GetHeight() - 40), 40, player1.rightImage[frame2]->GetHeight());
        else if (player2.sight == 3)
            g->DrawImage(player1.downImage[frame2], WIDTHGAP + (int)floor(player2.where.x), HEIGHTGAP + (int)floor(player2.where.y) - (player1.downImage[frame2]->GetHeight() - 40), 40, player1.downImage[frame2]->GetHeight());
    }
}