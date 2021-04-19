#pragma comment(lib, "ws2_32")
#include <winsock2.h>
#include <stdlib.h>
#include <stdio.h>
#include <windows.h>
#include "resource.h"

#define SERVERIP   "127.0.0.1"
#define SERVERPORT 9000
#define BUFSIZE    512
#define SIZE		16
#define INTROMSG "1. 회원가입\n2. 로그인\n3. 종료\n숫자를 입력하세요 >>"
#define SIGNMSG "\n[회원가입 페이지]\n"
#define LOGINMSG "\n[로그인 페이지]\n"
#define PAUSEMSG "연결 대기중..."

BOOL CALLBACK DlgProc2(HWND hDlg, UINT iMessage, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK DlgProc(HWND hDlg, UINT iMessage, WPARAM wParam, LPARAM lParam);
DWORD WINAPI ClientMain(LPVOID arg);										// 메인 클라이언트
DWORD WINAPI TetrisInfoRecvThread(LPVOID arg);								// 테트리스 받기 스레드
DWORD WINAPI TetrisInfoSendThread(LPVOID arg);								// 테트리스 주기 스레드
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
HINSTANCE g_hInst;
HWND	  hWndMain, hWndDlg;
LPCTSTR   lpszClass = TEXT("18032048 게플3A 신준철 기말고사");
HANDLE hClientThread, TetrisRecvThread, TetrisSendThread;

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdParam, int nCmdShow)
{

	HWND hWnd;
	MSG Message;
	WNDCLASS WndClass;
	g_hInst = hInstance;

	WndClass.cbClsExtra = 0;
	WndClass.cbWndExtra = 0;
	WndClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	WndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	WndClass.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(NULL));
	WndClass.hInstance = hInstance;
	WndClass.lpfnWndProc = WndProc;
	WndClass.lpszClassName = lpszClass;
	WndClass.lpszMenuName = MAKEINTRESOURCE(IDR_MENU1);
	WndClass.style = 0;
	RegisterClass(&WndClass);

	hWnd = CreateWindow(lpszClass, lpszClass, WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
		CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, NULL, (HMENU)NULL, hInstance, NULL);
	ShowWindow(hWnd, nCmdShow);


	DWORD ThreadId;
	hClientThread = CreateThread(NULL, 0, ClientMain, NULL, 0, &ThreadId);


	HACCEL hAccel = LoadAccelerators(hInstance, MAKEINTRESOURCE(NULL));
	while (GetMessage(&Message, NULL, 0, 0))
	{
		if (!TranslateAccelerator(hWnd, hAccel, &Message)) 
		{
			TranslateMessage(&Message);
			DispatchMessage(&Message);
		}
	}

	return (int)Message.wParam;

}

#define random(n) (rand()%n)
#define TS 24


// 좌표
struct Point {
	int x, y;
};

// 테트리스 모형
Point Shape[][4][4] = {
{	{0,0,1,0,2,0,-1,0}, {0,0,0,1,0,-1,0,-2},{0,0,1,0,2,0,-1,0},{0,0,0,1,0,-1,0,-2}	},
{	{0,0,1,0,0,1,1,1}, {0,0,1,0,0,1,1,1}, {0,0,1,0,0,1,1,1},{0,0,1,0,0,1,1,1}		},
{	{0,0,-1,0,0,-1,1,-1}, {0,0,0,1,-1,0,-1,-1}, {0,0,-1,0,0,-1,1,-1}, {0,0,0,1,-1,0,-1,-1} },
{	{0,0,-1,-1,0,-1,1,0}, {0,0,-1,0,-1,1,0,-1}, {0,0,-1,-1,0,-1,1,0}, {0,0,-1,0,-1,1,0,-1} },

{	{0,0,-1,0,1,0,-1,-1}, {0,0,0,-1,0,1,-1,1}, {0,0,-1,0,1,0,1,1}, {0,0,0,-1,0,1,1,-1} },
{	{0,0,1,0,-1,0,1,-1}, {0,0,0,1,0,-1,-1,-1}, {0,0,1,0,-1,0,-1,1}, {0,0,0,-1,0,1,1,1} },
{	{0,0,-1,0,1,0,0,1}, {0,0,0,-1,0,1,1,0}, {0,0,-1,0,1,0,0,-1}, {0,0,-1,0,0,-1,0,1} },
};

// 테트리스 게임 정보 구조체
struct _Tetris_info
{
	int board[22][34];		// 내 테트리스 보드
	int brick, nbrick;
	int nx, ny;				// x 위치값		// y 위치값
	int rot;				// 블록 회전 정보
	int score;
	int bricknum;

	bool setBrick = false;
};


// 벽돌, 벽, 빈 공간
enum TETRIS{
	EMPTY,
	BRICK,
	WALL = sizeof(Shape) / sizeof(Shape[0]) + 1,
};


// 내 닉네임과 상대 닉네임
TCHAR myNickName[SIZE];
TCHAR EnemyNickName[SIZE];

// 내 정보와 상대 정보
_Tetris_info* myGameInfo = new _Tetris_info();
_Tetris_info* enemyGameInfo = new _Tetris_info();

int arBW[] = { 8,10,12,15,20 };
int arBH[] = { 15,20,25,30,32 };
int BW = 15;
int BH = 30;

bool start = false;
bool gameflag = false;

HWND hEdit1, hEdit2, hEdit3, hText;
SOCKET c_sock;
RECT crt;

BOOL bShowSpace = TRUE;

// 게임 State
enum tag_Status {
	GAMEOVER,
	RUNNING,
	MATCHING,
	PAUSE,
};

tag_Status GameStatus;

// bitmap 정보
int interval;
HBITMAP hBit[9];

enum TETRISRESULT
{
	NORMAL,						// 일반적 패킷
	//NEW_BLOCK,				// 새로운 블록을 주는 패킷
	//FULL,						// 기록을 지워주는 패킷
};

enum PROTOCOL {					// 프로토콜
	NO_PROTOCOL = -1,
	JOIN_INFO,					// 회원가입 입력
	LOGIN_INFO,					// 로그인 입력
	JOIN_RESULT,				// 회원가입 결과
	LOGIN_RESULT,				// 로그인 결과
	GAME_INFO,					// 로그인이 성공적으로 됐을 경우
	GAME_INIT,					// 게임 초기화
	GAME_COUNTDOWN,				// 카운트 다운
	PLAYING_SEND,				// 정보 주기
	PLAYING_RECV,				// 정보 받기
	ENEMY_BLOCKSET,				// 상대 블록 정보
	GAME_END					// 게임 끝
};

PROTOCOL	protocol = PROTOCOL::NO_PROTOCOL;
int t_result;

enum RESULT {
	NODATA = -1,
	ID_EXIST = 1,				// 존재하는 아이디
	ID_ERROR,					// 아이디 에러
	PW_ERROR,					// 비밀번호 에러
	JOIN_SUCCESS,				// 회원가입 성공적
	LOGIN_SUCCESS,				// 로그인 성공적
	MATCHING_WAIT,				// 매칭 대기중
	MATCHING_SUCCESS,			// 매칭 성공
	COUNTING,					// 카운트다운 중..
	COUNTING_END,				// 카운트다운 끝
};

enum STATE {
	MENU_SELECT_STATE = 1,
	RESULT_SEND_STATE,
	DISCONNECTED_STATE
}; 

struct _User_Info
{
	char id[SIZE];
	char pw[SIZE];
	char nickname[SIZE];
};

char msg[BUFSIZE + 1];
_User_Info info;

void AdjustMainWindow();						// 윈도우 사이즈 조절
void DrawScreen(HDC hdc);						// 내 게임판 그리기
void DrawOtherScreen(HDC hdc);					// 상대 그림판 그리기
void MakeNewBrick(_Tetris_info* info);			// 새로운 블록 만들기 함수
int GetAround(int x, int y, int b, int r, _Tetris_info* info);		// 블록이 움직일수 있거나 or 움직일수 없거나
BOOL MoveDown(_Tetris_info* info);									// 블록을 내리는 함수
void TestFull(_Tetris_info* info);									// 블록 지우기 함수
void PrintTile(HDC hdc, int x, int y, int c);						// 블록 그리기 함수
void DrawBitmap(HDC hdc, int x, int y, HBITMAP hBit);				// 비트맵 그리기 함수
void UpdateBoard();													// 보드 업데이트
BOOL isMovingBrick(int x, int y, _Tetris_info* info);				// 블록이 움직이는 중이라면..
void EnemyBoardCheck(_Tetris_info* info);							// 상대 보드판 블록 체크

// 소켓 함수 오류 출력 후 종료
void err_quit(char* msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	MessageBox(NULL, (LPCTSTR)lpMsgBuf, msg, MB_ICONERROR);
	LocalFree(lpMsgBuf);
	exit(1);
}

// 소켓 함수 오류 출력
void err_display(char* msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	printf("[%s] %s", msg, (char*)lpMsgBuf);
	LocalFree(lpMsgBuf);
}

// 사용자 정의 데이터 수신 함수
int recvn(SOCKET s, char* buf, int len, int flags)
{
	int received;
	char* ptr = buf;
	int left = len;

	while (left > 0) {
		received = recv(s, ptr, left, flags);
		if (received == SOCKET_ERROR)
			return SOCKET_ERROR;
		else if (received == 0)
			break;
		left -= received;
		ptr += received;
	}

	return (len - left);
}
bool GetRetval(SOCKET _client_sock, char* _buf) {
	int retval;
	int size;
	retval = recvn(_client_sock, (char*)&size, sizeof(int), 0);
	if (retval == SOCKET_ERROR) {
		err_display("recv()");
		return false;
	}
	else if (retval == 0) return false;
	retval = recvn(_client_sock, _buf, size, 0);
	if (retval == SOCKET_ERROR) {
		err_display("recv()");
		return false;
	}
	else if (retval == 0)return false;

	return true;
}
PROTOCOL GetProtocol(char* _buf) {
	PROTOCOL protocol;
	memcpy(&protocol, _buf, sizeof(enum PROTOCOL));
	return protocol;
}
RESULT GetResult(char* _buf) {
	const char* ptr = _buf;
	ptr = ptr + sizeof(enum PROTOCOL);
	RESULT result;
	memcpy(&result, _buf, sizeof(enum RESULT));
	return result;
}
void msg_unpacking(const char* buf) {
	int strsize;
	const char* ptr = buf;
	ptr = ptr + sizeof(enum PROTOCOL);

	memcpy(&strsize, ptr, sizeof(int));
	ptr = ptr + sizeof(int);
	memcpy(msg, ptr, strsize);

	msg[strsize] = '\0';

	//printf("%s", msg);
}
void msg_unpacking(const char* buf, int *result) {
	int strsize;
	const char* ptr = buf;
	ptr = ptr + sizeof(enum PROTOCOL);

	memcpy(result, ptr, sizeof(enum RESULT));
	ptr = ptr + sizeof(enum RESULT);
	memcpy(&strsize, ptr, sizeof(int));
	ptr = ptr + sizeof(int);
	memcpy(msg, ptr, strsize);

	msg[strsize] = '\0';

	//printf("%s", msg);
}
void unpacking(const char* buf, char* EnemyNickName, struct _Tetris_info* myinfo, struct _Tetris_info* enemyinfo)
{
	int strsize;
	const char* ptr = buf;
	ptr = ptr + sizeof(enum PROTOCOL);
	ptr = ptr + sizeof(enum RESULT);

	memcpy(&strsize, ptr, sizeof(int));
	ptr = ptr + sizeof(int);

	memcpy(EnemyNickName, ptr, strsize);
	ptr = ptr + strsize;

	EnemyNickName[strsize] = '\0';


	memcpy(&myinfo->brick, ptr, sizeof(int));
	ptr = ptr + sizeof(int);

	memcpy(&myinfo->nbrick, ptr, sizeof(int));
	ptr = ptr + sizeof(int);

	memcpy(&myinfo->nx, ptr, sizeof(int));
	ptr = ptr + sizeof(int);

	memcpy(&myinfo->ny, ptr, sizeof(int));
	ptr = ptr + sizeof(int);

	memcpy(&myinfo->rot, ptr, sizeof(int));
	ptr = ptr + sizeof(int);

	memcpy(&myinfo->score, ptr, sizeof(int));
	ptr = ptr + sizeof(int);

	memcpy(&myinfo->bricknum, ptr, sizeof(int));
	ptr = ptr + sizeof(int);


	memcpy(&enemyinfo->brick, ptr, sizeof(int));
	ptr = ptr + sizeof(int);

	memcpy(&enemyinfo->nbrick, ptr, sizeof(int));
	ptr = ptr + sizeof(int);

	memcpy(&enemyinfo->nx, ptr, sizeof(int));
	ptr = ptr + sizeof(int);

	memcpy(&enemyinfo->ny, ptr, sizeof(int));
	ptr = ptr + sizeof(int);

	memcpy(&enemyinfo->rot, ptr, sizeof(int));
	ptr = ptr + sizeof(int);

	memcpy(&enemyinfo->score, ptr, sizeof(int));
	ptr = ptr + sizeof(int);

	memcpy(&enemyinfo->bricknum, ptr, sizeof(int));
	ptr = ptr + sizeof(int);

}
void unpacking(const char* buf, int count)
{
	int strsize;
	const char* ptr = buf;
	ptr = ptr + sizeof(enum PROTOCOL);
	ptr = ptr + sizeof(enum RESULT);

	memcpy(&count, ptr, sizeof(int));
	ptr = ptr + sizeof(int);

	itoa(count, msg, 10);
}
void unpacking(const char* buf)
{
	int strsize;
	const char* ptr = buf;
	ptr = ptr + sizeof(enum PROTOCOL);
	ptr = ptr + sizeof(enum RESULT);

	memcpy(&strsize, ptr, sizeof(int));
	ptr = ptr + sizeof(int);

	memcpy(msg, ptr, strsize);
	ptr = ptr + strsize;

	msg[strsize] = '\0';
}
void unpacking(const char* buf, int* result)
{
	const char* ptr = buf;
	ptr = ptr + sizeof(enum PROTOCOL);

	memcpy(result, ptr, sizeof(enum RESULT));
	ptr = ptr + sizeof(enum RESULT);
}
void unpacking(const char* buf, struct _Tetris_info* enemyinfo)
{
	const char* ptr = buf;
	ptr = ptr + sizeof(enum PROTOCOL);
	ptr = ptr + sizeof(enum TETRISRESULT);

	memcpy(&enemyinfo->nbrick, ptr, sizeof(int));
	ptr = ptr + sizeof(int);

	memcpy(&enemyinfo->brick, ptr, sizeof(int));
	ptr = ptr + sizeof(int);

	memcpy(&enemyinfo->nx, ptr, sizeof(int));
	ptr = ptr + sizeof(int);

	memcpy(&enemyinfo->ny, ptr, sizeof(int));
	ptr = ptr + sizeof(int);

	memcpy(&enemyinfo->rot, ptr, sizeof(int));
	ptr = ptr + sizeof(int);

	memcpy(&enemyinfo->score, ptr, sizeof(int));
	ptr = ptr + sizeof(int);

	memcpy(&enemyinfo->bricknum, ptr, sizeof(int));
	ptr = ptr + sizeof(int);
	

}
void Myinfounpacking(const char* buf, struct _Tetris_info* myInfo)
{
	const char* ptr = buf;
	ptr = ptr + sizeof(enum PROTOCOL);

	memcpy(&myInfo->setBrick, ptr, sizeof(bool));
}
int packing(enum PROTOCOL protocol, const char* number, char* buf) 
{
	int strsize;
	int size = 0;
	char* ptr;
	strsize = strlen(number);

	ptr = buf + sizeof(int);			// 총사이즈의 자리를 남겨둔다.

	memcpy(ptr, &protocol, sizeof(enum PROTOCOL));		// 프로토콜 패킹
	size = size + sizeof(enum PROTOCOL);				// 사이즈 증가
	ptr = ptr + sizeof(enum PROTOCOL);					// 위치 이동

	memcpy(ptr, &strsize, sizeof(int));
	size = size + sizeof(int);
	ptr = ptr + sizeof(int);

	memcpy(ptr, number, strsize);
	size = size + strsize;

	ptr = buf;											// 모든 작업이 끝나면 처음 자리로 돌아온다.		
	memcpy(ptr, &size, sizeof(int));					// 총 사이즈를 입력한다.

	return size + sizeof(int);
}
int packing(enum PROTOCOL protocol, struct _User_Info info, char* buf)
{
	int idsize = strlen(info.id);
	int pwsize = strlen(info.pw);
	int nicksize = strlen(info.nickname);

	char* ptr;
	int size = 0;
	ptr = buf + sizeof(int);							// 총사이즈의 자리를 남겨둔다.

	memcpy(ptr, &protocol, sizeof(enum PROTOCOL));		// 프로토콜 패킹
	size = size + sizeof(enum PROTOCOL);				// 사이즈 증가
	ptr = ptr + sizeof(enum PROTOCOL);					// 위치 이동

	memcpy(ptr, &idsize, sizeof(int));					// 아이디의 사이즈
	size = size + sizeof(int);							// 사이즈 증가
	ptr = ptr + sizeof(int);							// 위치 이동

	memcpy(ptr, info.id, idsize);						// 아이디
	size = size + idsize;								// 사이즈 증가
	ptr = ptr + idsize;									// 위치 이동

														// 패스워드도 마찬가지로 패킹
	memcpy(ptr, &pwsize, sizeof(int));
	size = size + sizeof(int);
	ptr = ptr + sizeof(int);

	memcpy(ptr, info.pw, pwsize);
	size = size + pwsize;
	ptr = ptr + pwsize;


	memcpy(ptr, &nicksize, sizeof(int));					
	size = size + sizeof(int);							
	ptr = ptr + sizeof(int);					

	memcpy(ptr, info.nickname, nicksize);			
	size = size + nicksize;							
	ptr = ptr + nicksize;								

	ptr = buf;										
	memcpy(ptr, &size, sizeof(int));				

	return size + sizeof(int);							// 총 사이즈 리턴
}
int packing(enum PROTOCOL protocol, char* buf) {
	char* ptr;
	int size = 0;
	ptr = buf + sizeof(int);

	memcpy(ptr, &protocol, sizeof(enum PROTOCOL));
	size = size + sizeof(enum PROTOCOL);
	ptr = ptr + sizeof(enum PROTOCOL);

	ptr = buf;
	memcpy(ptr, &size, sizeof(int));

	return size + sizeof(int);
}
int packing(enum PROTOCOL protocol, struct _Tetris_info* info, char* buf)
{
	char* ptr;
	int size = 0;
	ptr = buf + sizeof(int);

	memcpy(ptr, &protocol, sizeof(enum PROTOCOL));
	size = size + sizeof(enum PROTOCOL);
	ptr = ptr + sizeof(enum PROTOCOL);

	memcpy(ptr, &info->nbrick, sizeof(int));
	size = size + sizeof(int);
	ptr = ptr + sizeof(int);

	memcpy(ptr, &info->brick, sizeof(int));
	size = size + sizeof(int);
	ptr = ptr + sizeof(int);

	memcpy(ptr, &info->nx, sizeof(int));
	size = size + sizeof(int);
	ptr = ptr + sizeof(int);

	memcpy(ptr, &info->ny, sizeof(int));
	size = size + sizeof(int);
	ptr = ptr + sizeof(int);

	memcpy(ptr, &info->rot, sizeof(int));
	size = size + sizeof(int);
	ptr = ptr + sizeof(int);

	memcpy(ptr, &info->score, sizeof(int));
	size = size + sizeof(int);
	ptr = ptr + sizeof(int);

	memcpy(ptr, &info->bricknum, sizeof(int));
	size = size + sizeof(int);
	ptr = ptr + sizeof(int);

	memcpy(ptr, &info->setBrick, sizeof(bool));
	size = size + sizeof(bool);
	ptr = ptr + sizeof(bool);


	ptr = buf;
	memcpy(ptr, &size, sizeof(int));

	return size + sizeof(int);

}
int packing(enum PROTOCOL protocol, bool _set, char* buf)
{
	char* ptr;
	int size = 0;
	ptr = buf + sizeof(int);

	memcpy(ptr, &protocol, sizeof(enum PROTOCOL));
	size = size + sizeof(enum PROTOCOL);
	ptr = ptr + sizeof(enum PROTOCOL);

	memcpy(ptr, &_set, sizeof(bool));
	size = size + sizeof(bool);
	ptr = ptr + sizeof(bool);

	ptr = buf;
	memcpy(ptr, &size, sizeof(int));

	return size + sizeof(int);

}
void BoardInit()
{

	for (int x = 0; x < BW + 2; x++)
	{
		for (int y = 0; y < BH + 2; y++)
		{
			myGameInfo->board[x][y] = (y == 0 || y == BH + 1 || x == 0 || x == BW + 1) ? WALL : EMPTY;
			enemyGameInfo->board[x][y] = (y == 0 || y == BH + 1 || x == 0 || x == BW + 1) ? WALL : EMPTY;
		}
	}

}		// 보드 초기화					// 보드 초기화

DWORD WINAPI ClientMain(LPVOID arg)
{
	int retval;

	// 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	// socket()
	SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == INVALID_SOCKET) err_quit("socket()");

	// connect()
	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = inet_addr(SERVERIP);
	serveraddr.sin_port = htons(SERVERPORT);
	retval = connect(sock, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR) err_quit("connect()");

	// 데이터 통신에 사용할 변수
	char buf[BUFSIZE + 1];
	int len;
	int size;
	STATE state = STATE::MENU_SELECT_STATE;;
	DWORD ThreadId2, ThreadId3;

	char id[SIZE];
	char pw[SIZE];
	char nickname[SIZE];

	int count = 0;
	c_sock = sock;

	// 서버와 데이터 통신
	while (1) {
		char number[BUFSIZE + 1];
		int result;

		if(start) {
			// 데이터 받기
			if (!GetRetval(sock, buf))break;
		}

		switch (state) {
		case STATE::MENU_SELECT_STATE:								// 메뉴 선택
			switch (protocol)
			{
			case PROTOCOL::LOGIN_INFO:								// 로그인
				size = packing(PROTOCOL::LOGIN_INFO, info, buf);
				retval = send(sock, buf, size, 0);
				if (retval == SOCKET_ERROR) {
					err_quit("send()");
					break;
				}
				state = STATE::RESULT_SEND_STATE;
				start = true;
				break;
			case PROTOCOL::JOIN_INFO:								// 회원가입
				size = packing(PROTOCOL::JOIN_INFO, info, buf);
				retval = send(sock, buf, size, 0);
				if (retval == SOCKET_ERROR) {
					err_display("send()");
					break;
				}
				state = STATE::RESULT_SEND_STATE;
				start = true;
				break;
			case PROTOCOL::GAME_INFO:								// 로그인 성공적일때
				size = packing(PROTOCOL::GAME_INFO, buf);	
				retval = send(sock, buf, size, 0);
				if (retval == SOCKET_ERROR) {
					err_display("send()");
					break;
				}
				state = STATE::RESULT_SEND_STATE;
				start = true;
				break;
			case PROTOCOL::GAME_COUNTDOWN:							// 카운트 다운
				size = packing(PROTOCOL::GAME_COUNTDOWN, buf);
				retval = send(sock, buf, size, 0);
				if (retval == SOCKET_ERROR) {
					err_display("send()");
					break;
				}
				state = STATE::RESULT_SEND_STATE;
				start = true;
				break;
			case PROTOCOL::PLAYING_SEND:							// 게임중 정보구조체 보내기
				if (GameStatus != GAMEOVER) 
				{
					size = packing(PROTOCOL::PLAYING_SEND, myGameInfo, buf);
					retval = send(sock, buf, size, 0);
					if (retval == SOCKET_ERROR) 
					{
						err_display("send()");
						break;
					}
					start = false;
					protocol = PROTOCOL::NO_PROTOCOL;
				}
				else 
				{
					size = packing(PROTOCOL::GAME_END, buf);
					retval = send(sock, buf, size, 0);
					if (retval == SOCKET_ERROR) 
					{
						err_display("send()");
						break;
					}					
					start = false;
					protocol = PROTOCOL::NO_PROTOCOL;
				}
				break;
			case PROTOCOL::GAME_END:
				break;
			}
			break;
		case STATE::RESULT_SEND_STATE:
			switch (protocol = GetProtocol(buf))
			{
			case PROTOCOL::LOGIN_RESULT:							// 로그인 결과
				msg_unpacking(buf, &result);
				SetDlgItemText(hWndDlg, IDC_MSG, msg);
				switch (result) 
				{
				case RESULT::LOGIN_SUCCESS:							// 로그인 성공
					EndDialog(hWndDlg, 0);
					AdjustMainWindow();
					GameStatus = tag_Status::MATCHING;
					protocol = PROTOCOL::GAME_INFO;
					InvalidateRect(hWndMain, NULL, TRUE);
					start = false;
					break;
				default:
					start = false;
					break;
				}
				state = STATE::MENU_SELECT_STATE;
				break;
			case PROTOCOL::JOIN_RESULT:								// 회원가입 결과
				msg_unpacking(buf, &result);
				SetDlgItemText(hWndDlg, IDC_MSG, msg);
				start = false;
				state = STATE::MENU_SELECT_STATE;
				break;
			case PROTOCOL::GAME_INIT:								// 게임 초기화
				unpacking(buf, &result);
				strcpy(myNickName, info.nickname);
				switch (result)
				{
				case RESULT::MATCHING_WAIT:							// 매칭 대기중
					start = true;
					break;
				case RESULT::MATCHING_SUCCESS:						// 매칭 성공 시 카운트 다운
					BoardInit();
					unpacking(buf, EnemyNickName, myGameInfo, enemyGameInfo);
					GameStatus = tag_Status::PAUSE;
					protocol = PROTOCOL::GAME_COUNTDOWN;
					state = STATE::MENU_SELECT_STATE;
					start = false;
					InvalidateRect(hWndMain, NULL, TRUE);
					break;
				}
				break;
			case PROTOCOL::GAME_COUNTDOWN:							// 카운트 다운
				unpacking(buf, &result);
				InvalidateRect(hWndMain, NULL, TRUE);
				switch (result)
				{
				case RESULT::COUNTING:								// 카운트다운 중..
					unpacking(buf, count);
					protocol = PROTOCOL::GAME_COUNTDOWN;
					state = STATE::MENU_SELECT_STATE;
					start = false;
					Sleep(1000);
					break;
				case RESULT::COUNTING_END:							// 카운트다운 끝
					unpacking(buf);
					protocol = PROTOCOL::PLAYING_SEND;
					state = STATE::MENU_SELECT_STATE;
					start = false;
					Sleep(1000);
					InvalidateRect(hWndMain, NULL, TRUE);
					interval = 1000;
					SetTimer(hWndMain, 1, interval, NULL); 
					GameStatus = tag_Status::RUNNING;

					TetrisRecvThread = CreateThread(NULL, 0, TetrisInfoRecvThread, (LPVOID)sock, 0, &ThreadId2);
					TetrisSendThread = CreateThread(NULL, 0, TetrisInfoSendThread, (LPVOID)sock, 0, &ThreadId3);
					break;
				}
				break;
			case PROTOCOL::GAME_END:								// 게임 끝
				InvalidateRect(hWndMain, NULL, TRUE);
				KillTimer(hWndMain, 1);
				GameStatus = GAMEOVER;

				SetRect(&crt, 0, 0, 520, 150);
				AdjustWindowRect(&crt, WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, TRUE);
				SetWindowPos(hWndMain, NULL, 0, 0, crt.right - crt.left, crt.bottom - crt.top,
					SWP_NOMOVE | SWP_NOZORDER);

				MessageBox(hWndMain, TEXT("게임이 끝났습니다. 다시 시작하려면 게임/시작")
					TEXT(" 항목 (S)을 선택해 주십시오."), TEXT("알림"), MB_OK);
				break;
			}
			break;
		}

	}
	// closesocket()
	closesocket(sock);
	// 윈속 종료
	WSACleanup(); 
	return 0;
}

DWORD WINAPI TetrisInfoSendThread(LPVOID arg)
{
	SOCKET sock = (SOCKET)arg;
	int size;
	int retval;
	char buf[BUFSIZE + 1];

	while (1) {
		switch (protocol)						// 상대의 블록이 내려와서 쌓이는 경우 서버에게 bool을 보낸다.
		{
		case PROTOCOL::ENEMY_BLOCKSET:
			size = packing(PROTOCOL::ENEMY_BLOCKSET, enemyGameInfo->setBrick, buf);
			retval = send(sock, buf, size, 0);
			if (retval == SOCKET_ERROR)
			{
				err_display("send()");
				break;
			}
			protocol = PROTOCOL::NO_PROTOCOL;
		}
	}
}

DWORD WINAPI TetrisInfoRecvThread(LPVOID arg)
{
	SOCKET sock = (SOCKET)arg;


	PROTOCOL t_protocol;
	int result;
	char buf[BUFSIZE + 1];

	while (1) {
		if (!GetRetval(sock, buf))break;

		
		switch (t_protocol = GetProtocol(buf))						// 상대 블록이 쌓였다는 정보 or 상대의 현재정보를 받아오기
		{
		case PROTOCOL::ENEMY_BLOCKSET:
			UpdateBoard();
			Myinfounpacking(buf, myGameInfo);
			InvalidateRect(hWndMain, NULL, FALSE);
			break;
		case PROTOCOL::PLAYING_RECV:
			unpacking(buf, &result);
			switch (result)
			{
			case TETRISRESULT::NORMAL:
				UpdateBoard();
				unpacking(buf, enemyGameInfo);
				EnemyBoardCheck(enemyGameInfo);
				InvalidateRect(hWndMain, NULL, FALSE);
				break;
			}
			break;
		}
	}

	return 0;
}

TCHAR GameName[512];
TCHAR pause[128];

LRESULT CALLBACK WndProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam)
{
	int i;
	int trot;
	HDC hdc;
	PAINTSTRUCT ps;
	int x, y;
	static RECT rt;

	switch (iMessage)
	{
	case WM_CREATE:
		GetClientRect(hWnd, &crt);
		srand(GetTickCount());
		hWndMain = hWnd;
		GameStatus = GAMEOVER;
		rt = { 0, 0, 546, 146 };
		SetRect(&crt, 0, 0, 546, 146);
		AdjustWindowRect(&crt, WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, TRUE);
		SetWindowPos(hWndMain, NULL, 0, 0, crt.right - crt.left, crt.bottom - crt.top,
			SWP_NOMOVE | SWP_NOZORDER);

		strcpy(GameName, "▣▣▣▣▣▣▣▣▣▣▣▣▣▣▣▣▣▣▣▣▣▣▣▣▣▣▣▣▣▣▣▣▣▣");
		strcat(GameName, "▣　　　　　　　　　　　　　　　　　　　　　　　　　　　　　　　　▣");
		strcat(GameName, "▣　▣▣▣▣▣　▣▣▣▣　▣▣▣▣▣　▣▣▣　　　▣　　▣▣▣▣　▣");
		strcat(GameName, "▣　　　▣　　　▣　　　　　　▣　　　▣　　▣　　▣　　▣　　　　▣");
		strcat(GameName, "▣　　　▣　　　▣▣▣▣　　　▣　　　▣▣▣▣　　▣　　▣▣▣▣　▣");
		strcat(GameName, "▣　　　▣　　　▣　　　　　　▣　　　▣　▣　　　▣　　　　　▣　▣");
		strcat(GameName, "▣　　　▣　　　▣▣▣▣　　　▣　　　▣　　▣　　▣　　▣▣▣▣　▣");
		strcat(GameName, "▣　　　　　　　　　　　　　　　　　　　　　　　　　　　　　　　　▣");
		strcat(GameName, "▣▣▣▣▣▣▣▣▣▣▣▣▣▣▣▣▣▣▣▣▣▣▣▣▣▣▣▣▣▣▣▣▣▣");

		for (int i = 0; i < 9; i++)
		{
			hBit[i] = LoadBitmap(g_hInst, MAKEINTRESOURCE(IDB_BITMAP1 + i));
		}
		return 0;

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case ID_MENU_LOGIN:
			DialogBox(g_hInst, MAKEINTRESOURCE(IDD_DIALOG1), NULL, (DLGPROC)DlgProc);
			break;
		case ID_MENU_JOIN:
			DialogBox(g_hInst, MAKEINTRESOURCE(IDD_DIALOG2), NULL, (DLGPROC)DlgProc2);
			break;
		}
		return 0;

	case WM_TIMER:
		//EnemyBoardCheck(enemyGameInfo);
		if (MoveDown(myGameInfo) == TRUE && myGameInfo->setBrick)
		{
			myGameInfo->setBrick = false;
			MakeNewBrick(myGameInfo);
		}
		protocol = PROTOCOL::PLAYING_SEND;
		return 0;
	case WM_KEYDOWN:						// 방향키에따라 회전 or 블록 움직이기
		if (GameStatus != RUNNING || myGameInfo->brick == -1)
			return 0;
		switch (wParam)
		{
		case VK_LEFT:
			UpdateBoard();
			if (GetAround(myGameInfo->nx - 1, myGameInfo->ny, myGameInfo->brick, myGameInfo->rot, myGameInfo) == EMPTY) {
				myGameInfo->nx--;
				InvalidateRect(hWnd, NULL, FALSE);
				protocol = PROTOCOL::PLAYING_SEND;
			}					
			break;
		case VK_RIGHT:
			UpdateBoard();
			if (GetAround(myGameInfo->nx + 1, myGameInfo->ny, myGameInfo->brick, myGameInfo->rot, myGameInfo) == EMPTY)
			{
				myGameInfo->nx++;
				InvalidateRect(hWnd, NULL, FALSE);
				protocol = PROTOCOL::PLAYING_SEND;
			}
			break;
		case VK_UP:
			UpdateBoard();
			trot = (myGameInfo->rot == 3 ? 0 :myGameInfo->rot + 1);
			if (GetAround(myGameInfo->nx, myGameInfo->ny, myGameInfo->brick, trot, myGameInfo) == EMPTY)
			{
				myGameInfo->rot = trot;
				InvalidateRect(hWnd, NULL, FALSE);
				protocol = PROTOCOL::PLAYING_SEND;
			}
			break;
		case VK_DOWN:
			if (MoveDown(myGameInfo) == TRUE && myGameInfo->setBrick) {
				MakeNewBrick(myGameInfo);
				myGameInfo->setBrick = false;
			}
			protocol = PROTOCOL::PLAYING_SEND;
			break;
		}
		return 0;
	case WM_PAINT:									// 게임 state에 따라 그리는 것을 다르게 한다.
		hdc = BeginPaint(hWnd, &ps);

		switch (GameStatus)
		{
		case tag_Status::GAMEOVER:
			DrawText(hdc, GameName, lstrlen(GameName), &rt, DT_WORDBREAK);
			break;
		case tag_Status::MATCHING:
			strcpy(pause, PAUSEMSG);
			TextOut(hdc, ((BW + 10)* TS) * 2 / 2, (BH - 1)* TS / 2, pause, strlen(PAUSEMSG));
			break;
		case tag_Status::PAUSE:
			DrawScreen(hdc);
			DrawOtherScreen(hdc);
			TextOut(hdc, ((BW + 12)* TS) * 2 / 2, (BH - 1)* TS / 2, msg, strlen(msg));
			break;
		case tag_Status::RUNNING:
			DrawScreen(hdc);
			DrawOtherScreen(hdc);
			break;
		}

		EndPaint(hWnd, &ps);
		return 0;
	case WM_DESTROY:
		KillTimer(hWnd, 1);	
		CloseHandle(hClientThread);
		CloseHandle(TetrisRecvThread);
		CloseHandle(TetrisSendThread);
		closesocket(c_sock);
		WSACleanup();
		delete myGameInfo;
		delete enemyGameInfo;
		for (int i = 0; i < 9; i++)
		{
			DeleteObject(hBit[i]);
		}
		PostQuitMessage(0);
		// 윈속 종료
		return 0;
	}

	return(DefWindowProc(hWnd, iMessage, wParam, lParam));
}

// 로그인창
BOOL CALLBACK DlgProc(HWND hDlg, UINT iMessage, WPARAM wParam, LPARAM lParam)
{

	// 로그인 창
	hWndDlg = hDlg;

	switch (iMessage)
	{

	case WM_INITDIALOG:
		hEdit1 = GetDlgItem(hDlg, IDC_EDIT1);
		hEdit2 = GetDlgItem(hDlg, IDC_EDIT2);
		return TRUE;
	case WM_SYSCOMMAND:
		if (wParam == SC_CLOSE)
		{
			start = false;
			EndDialog(hDlg, 0);
		}
		return TRUE;
	case WM_COMMAND:
		switch (wParam)
		{
		case IDC_BUTTON1:
			GetDlgItemText(hDlg, IDC_EDIT1, info.id, SIZE + 1);
			GetDlgItemText(hDlg, IDC_EDIT2, info.pw, SIZE + 1);
			protocol = PROTOCOL::LOGIN_INFO;
			break;
		}
		return TRUE;

	case WM_ACTIVATE:
		return TRUE;
	}

	return FALSE;
}
// 회원가입 창
BOOL CALLBACK DlgProc2(HWND hDlg, UINT iMessage, WPARAM wParam, LPARAM lParam)
{
	hWndDlg = hDlg;

	switch (iMessage)
	{

	case WM_INITDIALOG:
		hEdit1 = GetDlgItem(hDlg, IDC_EDIT1);
		hEdit2 = GetDlgItem(hDlg, IDC_EDIT2);
		hEdit3 = GetDlgItem(hDlg, IDC_EDIT3);
		return TRUE;
	case WM_SYSCOMMAND:
		if (wParam == SC_CLOSE)
		{
			start = false;
			EndDialog(hDlg, 0);
		}
		return TRUE;
	case WM_COMMAND:
		switch (wParam)
		{
		case IDC_BUTTON1:
			GetDlgItemText(hDlg, IDC_EDIT1, info.id, SIZE + 1);
			GetDlgItemText(hDlg, IDC_EDIT2, info.pw, SIZE + 1);
			GetDlgItemText(hDlg, IDC_EDIT3, info.nickname, SIZE + 1);
			protocol = PROTOCOL::JOIN_INFO;
			break;
		}
		return TRUE;

	case WM_ACTIVATE:
		return TRUE;
	}

	return FALSE;
}


// 나머지 함수는 API책 참고
void DrawScreen(HDC hdc)
{
	int x, y, i;
	TCHAR str[128];

	for (x = 0; x < BW + 1; x++)
	{
		PrintTile(hdc, x, 0, WALL);
		PrintTile(hdc, x, BH + 1, WALL);
	}

	for (y = 0; y < BH + 2; y++)
	{
		PrintTile(hdc, 0, y, WALL);
		PrintTile(hdc, BW + 1, y, WALL);
	}

	for (x = 1; x < BW + 1; x++) {
		for (y = 1; y < BH + 1; y++) {
			if (isMovingBrick(x, y, myGameInfo)) {
				PrintTile(hdc, x, y, myGameInfo->brick + 1);
			}
			else {
				PrintTile(hdc, x, y, myGameInfo->board[x][y]);
			}
		}
	}

	if (GameStatus != GAMEOVER && myGameInfo->brick != -1) {
		for (i = 0; i < 4; i++) {
			PrintTile(hdc, myGameInfo->nx + Shape[myGameInfo->brick][myGameInfo->rot][i].x,
				myGameInfo->ny + Shape[myGameInfo->brick][myGameInfo->rot][i].y, myGameInfo->brick + 1);
		}
	}

	for (x = BW + 3; x <= BW + 11; x++) {
		for (y = BH - 5; y <= BH + 1; y++) {
			if (x == BW + 3 || x == BW + 11 || y == BH - 5 || y == BH + 1) {
				PrintTile(hdc, x, y, WALL);
			}
			else {
				PrintTile(hdc, x, y, 0);
			}
		}
	}

	if (GameStatus != GAMEOVER) {
		for (i = 0; i < 4; i++) {
			PrintTile(hdc, BW + 7 + Shape[myGameInfo->nbrick][0][i].x,
				BH - 2 + Shape[myGameInfo->nbrick][0][i].y,
				myGameInfo->nbrick + 1);
		}
	}

	lstrcpy(str, "닉네임 : ");
	lstrcat(str, myNickName);
	TextOut(hdc, (BW + 4) * TS, 30, str, lstrlen(str));
	wsprintf(str, TEXT("점수 : %d     "), myGameInfo->score);
	TextOut(hdc, (BW + 4) * TS, 60, str, lstrlen(str));
	wsprintf(str, TEXT("벽돌 : %d 개   "), myGameInfo->bricknum);
	TextOut(hdc, (BW + 4) * TS, 80, str, lstrlen(str));
}
void DrawOtherScreen(HDC hdc)
{
	int x, y, i;
	TCHAR str[128];
	bool flag = false;

	for (x = 0; x < BW + 1; x++)
	{
		PrintTile(hdc, x + (BW + 13), 0, WALL);
		PrintTile(hdc, x + (BW + 13), BH + 1, WALL);
	}

	for (y = 0; y < BH + 2; y++)
	{
		PrintTile(hdc, 0 + (BW + 13), y, WALL);
		PrintTile(hdc, BW + 1 + (BW + 13), y, WALL);
	}

	for (x = 1; x < BW + 1; x++) {
		for (y = 1; y < BH + 1; y++) {
			if (isMovingBrick(x, y, enemyGameInfo)) {
				PrintTile(hdc, x + (BW + 13), y, enemyGameInfo->brick + 1);
			}
			else {
				PrintTile(hdc, x + (BW + 13), y, enemyGameInfo->board[x][y]);
			}
		}
	}

	if (GameStatus != GAMEOVER && enemyGameInfo->brick != -1) {
		for (i = 0; i < 4; i++) {
			PrintTile(hdc, enemyGameInfo->nx + Shape[enemyGameInfo->brick][enemyGameInfo->rot][i].x + (BW + 13),
				enemyGameInfo->ny + Shape[enemyGameInfo->brick][enemyGameInfo->rot][i].y, enemyGameInfo->brick + 1);
		}
	}

	for (x = BW + 3; x <= (BW + 11); x++) {
		for (y = BH - 5; y <= BH + 1; y++) {
			if (x == BW + 3 || x == BW + 11 || y == BH - 5 || y == BH + 1) {
				PrintTile(hdc, x + (BW + 13), y, WALL);
			}
			else {
				PrintTile(hdc, x + (BW + 13), y, 0);
			}
		}
	}

	if (GameStatus != GAMEOVER) {
		for (i = 0; i < 4; i++) {
			PrintTile(hdc, BW + 7 + Shape[enemyGameInfo->nbrick][0][i].x + (BW + 13),
				BH - 2 + Shape[enemyGameInfo->nbrick][0][i].y,
				enemyGameInfo->nbrick + 1);
		}
	}

	lstrcpy(str, "닉네임 : ");
	lstrcat(str, EnemyNickName);
	TextOut(hdc, (BW + 9) * TS * 2, 30, str, lstrlen(str));
	wsprintf(str, TEXT("점수 : %d     "), enemyGameInfo->score);
	TextOut(hdc, (BW + 9) * TS * 2, 60, str, lstrlen(str));
	wsprintf(str, TEXT("벽돌 : %d 개   "), enemyGameInfo->bricknum);
	TextOut(hdc, (BW + 9) * TS * 2, 80, str, lstrlen(str));
}
void MakeNewBrick(_Tetris_info* info)
{
	info->bricknum++;
	info->brick = info->nbrick;
	info->nbrick = random(sizeof(Shape) / sizeof(Shape[0]));
	info->nx = BW / 2;
	info->ny = 3;
	info->rot = 0;
	InvalidateRect(hWndMain, NULL, FALSE);
	if (GetAround(info->nx, info->ny, info->brick, info->rot, info) != EMPTY) {	// 블록이 못내려가는경우
		InvalidateRect(hWndMain, NULL, TRUE);
		KillTimer(hWndMain, 1);
		GameStatus = GAMEOVER;

		SetRect(&crt, 0, 0, 520, 150);
		AdjustWindowRect(&crt, WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, TRUE);
		SetWindowPos(hWndMain, NULL, 0, 0, crt.right - crt.left, crt.bottom - crt.top,
			SWP_NOMOVE | SWP_NOZORDER);

		MessageBox(hWndMain, TEXT("게임이 끝났습니다. 다시 시작하려면 게임/시작")
			TEXT(" 항목 (S)을 선택해 주십시오."), TEXT("알림"), MB_OK);
	}
}
int GetAround(int x, int y, int b, int r, _Tetris_info* info)
{
	int i, k = EMPTY;

	for (i = 0; i < 4; i++) {
		k = max(k, info->board[x + Shape[b][r][i].x][y + Shape[b][r][i].y]);
	}

	return k;
}
BOOL MoveDown(_Tetris_info* info)
{
	if (GetAround(info->nx, info->ny + 1, info->brick, info->rot, info) != EMPTY) {
		TestFull(info);
		return TRUE;
	}
	info->ny++;
	InvalidateRect(hWndMain, NULL, FALSE);
	UpdateWindow(hWndMain);
	return FALSE;
}
void TestFull(_Tetris_info* info)
{
	int i, x, y, ty;
	int count = 0;
	static int arScoreInc[] = { 0,1,3,8,20 };

	for (i = 0; i < 4; i++) {
		info->board[info->nx + Shape[info->brick][info->rot][i].x]
			[info->ny + Shape[info->brick][info->rot][i].y] = info->brick + 1;
	}

	info->brick = -1;

	for (y = 1; y < BH+1; y++) {
		for (x = 1; x < BW + 1; x++) {
			if (info->board[x][y] == EMPTY) break;
		}

		if (x == BW + 1) {
			count++;
			for (ty = y; ty > 1; ty--) {
				for (x = 1; x < BW + 1; x++) {
					info->board[x][ty] = info->board[x][ty - 1];
				}
			}
			InvalidateRect(hWndMain, NULL, FALSE);
			UpdateBoard();
			UpdateWindow(hWndMain);
			Sleep(150);
		}
	}

	info->score += arScoreInc[count];
	if (info->bricknum % 10 == 0 && interval > 200) {
		interval -= 50;
		SetTimer(hWndMain, 1, interval, NULL);
	}
}
void DrawBitmap(HDC hdc, int x, int y, HBITMAP hBit) {

	HDC MemDC;
	HBITMAP OldBitmap;
	int bx, by;
	BITMAP bit;
	
	MemDC = CreateCompatibleDC(hdc);
	OldBitmap = (HBITMAP)SelectObject(MemDC, hBit);
	GetObject(hBit, sizeof(BITMAP), &bit);


	bx = bit.bmWidth;
	by = bit.bmHeight;

	BitBlt(hdc, x, y, bx, by, MemDC, 0, 0, SRCCOPY);

	SelectObject(MemDC, OldBitmap);
	DeleteDC(MemDC);
}
void PrintTile(HDC hdc, int x, int y, int c) {
	DrawBitmap(hdc, x * TS, y * TS, hBit[c]);

	if (c == EMPTY && bShowSpace)
	{
		Rectangle(hdc, x * TS + TS / 2 - 1, y * TS + TS / 2 - 1, x * TS + TS / 2 + 1, y * TS + TS / 2 + 1);
	}
	return;
}
void AdjustMainWindow()
{
	RECT crt;

	SetRect(&crt, 0, 0, ((BW + 13) * TS) * 2, (BH + 2) * TS);
	AdjustWindowRect(&crt, WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, TRUE);
	SetWindowPos(hWndMain, NULL, 0, 0, crt.right - crt.left, crt.bottom - crt.top,
		SWP_NOMOVE | SWP_NOZORDER);
}
void UpdateBoard()
{
	RECT rt;

	SetRect(&rt, TS, TS, (BW + 1) * TS, (BH + 1) * TS);
	InvalidateRect(hWndMain, &rt, FALSE);
}
BOOL isMovingBrick(int x, int y, _Tetris_info* info)
{
	int i;

	if (GameStatus == GAMEOVER || info->brick == -1) {
		return FALSE;
	}

	for (int i = 0; i < 4; i++) 
	{
		if (x == info->nx + Shape[info->brick][info->rot][i].x
			&& y == info->ny + Shape[info->brick][info->rot][i].y) {
			return TRUE;
		}
	}
	return FALSE;
}
void EnemyBoardCheck(_Tetris_info* info)
{
	if (GetAround(info->nx, info->ny + 1, info->brick, info->rot, info) != EMPTY) {
		TestFull(info);
		info->setBrick = true;
		protocol = PROTOCOL::ENEMY_BLOCKSET;
	}
	InvalidateRect(hWndMain, NULL, FALSE);
	UpdateWindow(hWndMain);
}