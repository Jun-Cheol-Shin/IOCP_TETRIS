#pragma comment(lib, "ws2_32.lib")
#include <winsock2.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#define SERVERPORT 9000
#define BUFSIZE 4096
#define IDSIZE 255
#define PWSIZE 255
#define NICKNAMESIZE 255

#define ID_ERROR_MSG "���� ���̵��Դϴ�\n"
#define PW_ERROR_MSG "�н����尡 Ʋ�Ƚ��ϴ�.\n"
#define LOGIN_SUCCESS_MSG "�α��ο� �����߽��ϴ�.\n"
#define ID_EXIST_MSG "�̹� �ִ� ���̵� �Դϴ�.\n"
#define JOIN_SUCCESS_MSG "���Կ� �����߽��ϴ�.\n"
#define random(n) (rand()%n)
#define BW 15
#define BH 30
#define COUNT 3

struct Point {
	int x, y;
};
Point Shape[][4][4] = {
{	{0,0,1,0,2,0,-1,0}, {0,0,0,1,0,-1,0,-2},{0,0,1,0,2,0,-1,0},{0,0,0,1,0,-1,0,-2}	},
{	{0,0,1,0,0,1,1,1}, {0,0,1,0,0,1,1,1}, {0,0,1,0,0,1,1,1},{0,0,1,0,0,1,1,1}		},
{	{0,0,-1,0,0,-1,1,-1}, {0,0,0,1,-1,0,-1,-1}, {0,0,-1,0,0,-1,1,-1}, {0,0,0,1,-1,0,-1,-1} },
{	{0,0,-1,-1,0,-1,1,0}, {0,0,-1,0,-1,1,0,-1}, {0,0,-1,-1,0,-1,1,0}, {0,0,-1,0,-1,1,0,-1} },
{	{0,0,-1,0,1,0,-1,-1}, {0,0,0,-1,0,1,-1,1}, {0,0,-1,0,1,0,1,1}, {0,0,0,-1,0,1,1,-1} },
{	{0,0,1,0,-1,0,1,-1}, {0,0,0,1,0,-1,-1,-1}, {0,0,1,0,-1,0,-1,1}, {0,0,0,-1,0,1,1,1} },
{	{0,0,-1,0,1,0,0,1}, {0,0,0,-1,0,1,1,0}, {0,0,-1,0,1,0,0,-1}, {0,0,-1,0,0,-1,0,1} },
};

enum TETRISRESULT
{
	NORMAL,					// �Ϲ��� ��Ŷ
	NEW_BLOCK,				// ���ο� ����� �ִ� ��Ŷ
	FULL,					// ����� �����ִ� ��Ŷ
};

enum RESULT
{
	NODATA = -1,
	ID_EXIST = 1,
	ID_ERROR,
	PW_ERROR,
	JOIN_SUCCESS,
	LOGIN_SUCCESS,
	MATCHING_WAIT,
	MATCHING_SUCCESS,
	COUNTING,
	COUNTING_END,
};
enum PROTOCOL
{
	JOIN_INFO,
	LOGIN_INFO,
	JOIN_RESULT,
	LOGIN_RESULT,
	GAME_INFO,
	GAME_INIT,
	GAME_COUNTDOWN,
	PLAYING_SEND,
	PLAYING_RECV,
	ENEMY_BLOCKSET,
	GAME_END
};

enum STATE
{
	MENU_SELECT_STATE = 1,
	RESULT_SEND_STATE,
	DISCONNECTED_STATE
};

enum
{
	SOC_ERROR = 1,
	SOC_TRUE,
	SOC_FALSE
};

struct _User_Info
{
	char id[IDSIZE];
	char pw[PWSIZE];
	char nickname[NICKNAMESIZE];
	bool state;
};

enum IO_TYPE
{
	IO_RECV = 1,
	IO_SEND
};

enum ROOM_STATE : int
{
	ROOM_WAIT = 1, // ��ٸ��� �ܰ�
	ROOM_PLAY, // ������
	ROOM_END // ���� ��
};

enum TETRIS {
	EMPTY,
	BRICK,
	WALL = sizeof(Shape) / sizeof(Shape[0]) + 1,
};

struct _Client_info;

struct WSAOVERLAPPED_EX
{
	WSAOVERLAPPED overlapped;
	_Client_info*	  ptr;
	IO_TYPE       type;
};

struct _Tetris_info
{
	int board[22][34];		// �� ��Ʈ���� ����
	int brick, nbrick;
	int nx, ny;				// x ��ġ��		// y ��ġ��
	int rot;				// ��� ȸ�� ����
	int score;
	int bricknum;


	bool setBrick = false;
};

struct _Client_info
{
	WSAOVERLAPPED_EX	r_overlapped;
	WSAOVERLAPPED_EX	s_overlapped;

	SOCKET			sock;
	SOCKADDR_IN		addr;
	_User_Info		userinfo;
	STATE			state;
	bool			r_sizeflag;

	int				recvbytes;
	int				comp_recvbytes;
	int				sendbytes;
	int				comp_sendbytes;

	char			recvbuf[BUFSIZE];
	char			sendbuf[BUFSIZE];

	WSABUF			r_wsabuf;
	WSABUF			s_wsabuf;

	bool			gamesend = false;

	_Tetris_info*	gameInfo;
};

struct _Room_Info 
{
	ROOM_STATE r_state;
	_Client_info* users[2];
	int count = 0;
};

// ȸ������ ����Ʈ
_User_Info* Join_List[100];
int Join_Count = 0;

// ������ ����Ʈ
_Client_info* Client_List[100];
int Client_Count = 0;

// �� ����Ʈ
_Room_Info* RoomInfo[50];
int room_count = 0;

CRITICAL_SECTION cs;

SOCKET client_sock;
HANDLE hReadEvent, hWriteEvent;
_Room_Info* roomptr;

void err_quit(char* msg);
void err_display(int errcode);
void err_display(char* msg);

void GetProtocol(char* _ptr, PROTOCOL& _protocol);
void PackPacket(char* _buf, PROTOCOL _protocol, RESULT _result, char* _str1, int& _size);
void PackPacket(char* _buf, PROTOCOL _protocol, RESULT _result, char* _str, struct _Tetris_info* info, _Tetris_info* info2, int& _size);
void PackPacket(char* _buf, PROTOCOL _protocol, RESULT _result, int& _size);
void PackPacket(char* _buf, PROTOCOL _protocol, RESULT _result, int _count, int& _size);
void PackPacket(char* _buf, PROTOCOL _protocol, TETRISRESULT _result, struct _Tetris_info* _info, int& _size);
void PackPacket(char* _buf, PROTOCOL _protocol, int& _size);
void PackPacket(char* _buf, PROTOCOL _protocol, bool _set, int& _size);
void UnPackPacket(char* _buf, char* _str1, char* _str2, char* _str3);
void UnPackPacket(char* _buf, char* _str1, char* _str2);
void UnPackPacket(char* _buf, _Tetris_info* info);
void UnPackEnemyPacket(char* _buf, _Tetris_info* e_info);

_Client_info* AddClientInfo(SOCKET _sock, SOCKADDR_IN _addr);
void RemoveClientInfo(_Client_info* _ptr);
void RecvProcess(_Client_info* _ptr);
void SendProcess(_Client_info* _ptr);
bool Recv(_Client_info* _ptr);
int CompleteRecv(_Client_info* _ptr, int _completebyte);
bool Send(_Client_info* _ptr, int _size);
int CompleteSend(_Client_info* _ptr, int _completebyte);


// new
_Room_Info* AddRoomInfo();											// �� �߰�
void RemoveRoomInfo(_Room_Info* _ptr);								// �� ����
_Room_Info* SearchEmptyRoom(_Client_info* _client);					// ��� ã��
bool Add_User(_Client_info* _data, _Room_Info* room);				// �濡 ����� �߰�
void Remove_User(_Client_info* _data);								// �濡 ����� ����
bool loginProcess(_Client_info* _ptr);								// �α��� ���μ���
bool JoinProcess(_Client_info* _ptr);								// ȸ������ ���μ���
bool GameInitProcess(_Client_info* _ptr);							// ���� �ʱ�ȭ ���μ���
bool CountDownProcess(_Client_info* _ptr);							// ī��Ʈ�ٿ� ���μ���
bool TetrisProcess(_Client_info* _ptr);								// ��Ʈ���� ���� ���μ���
bool EnemyBlockSetProcess(_Client_info* _ptr);						// ��� ����� ������ �������� ��� ���μ���
bool EndgameProcess(_Client_info* _ptr);							// ���� ���� ���μ���

// Game
void BoardInit(_Room_Info* _ptr);
void MakeBrick(_Tetris_info* info);

// �񵿱� ����� ���۰� ó�� �Լ�
DWORD WINAPI WorkerThread(LPVOID arg);
// ���� �Լ� ���� ��� �� ����

int main(int argc, char *argv[])
{
	int retval;

	// ���� �ʱ�ȭ
	WSADATA wsa;
	if(WSAStartup(MAKEWORD(2,2), &wsa) != 0) return 1;
	InitializeCriticalSection(&cs);
	// ����� �Ϸ� ��Ʈ ����
	HANDLE hcp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if(hcp == NULL) return 1;

	// CPU ���� Ȯ��
	SYSTEM_INFO si;
	GetSystemInfo(&si);

	// (CPU ���� * 2)���� �۾��� ������ ����
	HANDLE hThread;
	for(int i=0; i<(int)si.dwNumberOfProcessors*2; i++){
		hThread = CreateThread(NULL, 0, WorkerThread, hcp, 0, NULL);
		if(hThread == NULL) return 1;
		CloseHandle(hThread);
	}

	// socket()
	SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if(listen_sock == INVALID_SOCKET) err_quit("socket()");

	// bind()
	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(SERVERPORT);
	retval = bind(listen_sock, (SOCKADDR *)&serveraddr, sizeof(serveraddr));
	if(retval == SOCKET_ERROR) err_quit("bind()");
	
	// listen()
	retval = listen(listen_sock, SOMAXCONN);
	if(retval == SOCKET_ERROR) err_quit("listen()");

	// ������ ��ſ� ����� ����
	SOCKET client_sock;
	SOCKADDR_IN clientaddr;
	int addrlen;

	while(1){
		// accept()
		addrlen = sizeof(clientaddr);
		client_sock = accept(listen_sock, (SOCKADDR *)&clientaddr, &addrlen);
		if(client_sock == INVALID_SOCKET){
			err_display("accept()");
			break;
		}
		// ���ϰ� ����� �Ϸ� ��Ʈ ����
		CreateIoCompletionPort((HANDLE)client_sock, hcp, client_sock, 0);

		_Client_info* ptr = AddClientInfo(client_sock, clientaddr);

		if (!Recv(ptr))
		{
			continue;
		}
		
	}

	// ���� ����
	WSACleanup();
	return 0;
}

// �۾��� ������ �Լ�
DWORD WINAPI WorkerThread(LPVOID arg)
{

	srand((unsigned)time(NULL));
	int retval;
	HANDLE hcp = (HANDLE)arg;
	
	while(1)
	{
		// �񵿱� ����� �Ϸ� ��ٸ���
		DWORD cbTransferred;
		SOCKET client_sock;
		WSAOVERLAPPED_EX* overlapped;

		retval = GetQueuedCompletionStatus(hcp, &cbTransferred,
			(LPDWORD)&client_sock, (LPOVERLAPPED *)&overlapped, INFINITE);

		_Client_info* ptr = overlapped->ptr;

		// Ŭ���̾�Ʈ ���� ���
		SOCKADDR_IN clientaddr;
		int addrlen = sizeof(clientaddr);
		getpeername(ptr->sock, (SOCKADDR *)&clientaddr, &addrlen);
		
		// �񵿱� ����� ��� Ȯ��
		if(retval == 0 || cbTransferred == 0)
		{
			if(retval == 0)			
			{				
				DWORD temp1, temp2;
				WSAGetOverlappedResult(ptr->sock, &overlapped->overlapped,
					&temp1, FALSE, &temp2);
				err_display("WSAGetOverlappedResult()");
			}

			ptr->state = DISCONNECTED_STATE;			
		}


		if (ptr->state != DISCONNECTED_STATE)
		{
			switch (overlapped->type)
			{
			case IO_TYPE::IO_RECV:
			{
				int result = CompleteRecv(ptr, cbTransferred);
				switch (result)
				{
				case SOC_ERROR:
					continue;
				case SOC_FALSE:
					continue;
				case SOC_TRUE:
					break;
				}

				RecvProcess(ptr);		

				if (!Recv(ptr))
				{
					ptr->state = DISCONNECTED_STATE;
				}
			}
			break;
			case IO_TYPE::IO_SEND:
			{
				int result = CompleteSend(ptr, cbTransferred);
				switch (result)
				{
				case SOC_ERROR:
					continue;
				case SOC_FALSE:
					continue;
				case SOC_TRUE:
					break;
				}

				SendProcess(ptr);
			}
			break;
			}
		}

		if (ptr->state == DISCONNECTED_STATE)
		{
			Remove_User(ptr);
			RemoveClientInfo(ptr);
		}
		
	}
	return 0;
}


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
void err_display(int errcode)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, errcode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	printf("[����] %s", (char*)lpMsgBuf);
	LocalFree(lpMsgBuf);
}
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

void GetProtocol(char* _ptr, PROTOCOL& _protocol)
{
	memcpy(&_protocol, _ptr, sizeof(PROTOCOL));
}
void PackPacket(char* _buf, PROTOCOL _protocol, RESULT _result, char* _str1, int& _size)
{
	char* ptr = _buf;
	int strsize1 = strlen(_str1);
	_size = 0;

	ptr = ptr + sizeof(_size);

	memcpy(ptr, &_protocol, sizeof(_protocol));
	ptr = ptr + sizeof(_protocol);
	_size = _size + sizeof(_protocol);

	memcpy(ptr, &_result, sizeof(_result));
	ptr = ptr + sizeof(_result);
	_size = _size + sizeof(_result);

	memcpy(ptr, &strsize1, sizeof(strsize1));
	ptr = ptr + sizeof(strsize1);
	_size = _size + sizeof(strsize1);

	memcpy(ptr, _str1, strsize1);
	ptr = ptr + strsize1;
	_size = _size + strsize1;

	ptr = _buf;
	memcpy(ptr, &_size, sizeof(_size));

	_size = _size + sizeof(_size);
}
void PackPacket(char* _buf, PROTOCOL _protocol, RESULT _result, char* _str, struct _Tetris_info* info, _Tetris_info* info2, int& _size)
{
	int strsize = strlen(_str);
	char* ptr;
	_size = 0;
	ptr = _buf + sizeof(int);							// �ѻ������� �ڸ��� ���ܵд�.

	memcpy(ptr, &_protocol, sizeof(_protocol));			// �������� ��ŷ
	_size = _size + sizeof(_protocol);					// ������ ����
	ptr = ptr + sizeof(_protocol);						// ��ġ �̵�

	memcpy(ptr, &_result, sizeof(_result));
	ptr = ptr + sizeof(_result);
	_size = _size + sizeof(_result);

	memcpy(ptr, &strsize, sizeof(strsize));
	ptr = ptr + sizeof(strsize);
	_size = _size + sizeof(strsize);

	memcpy(ptr, _str, strsize);
	ptr = ptr + strsize;
	_size = _size + strsize;

	memcpy(ptr, &info->brick, sizeof(int));				// ���̵��� ������
	_size = _size + sizeof(int);							// ������ ����
	ptr = ptr + sizeof(int);								// ��ġ �̵�

	memcpy(ptr, &info->nbrick, sizeof(int));				// ���̵��� ������
	_size = _size + sizeof(int);							// ������ ����
	ptr = ptr + sizeof(int);								// ��ġ �̵�

	memcpy(ptr, &info->nx, sizeof(int));
	_size = _size + sizeof(int);
	ptr = ptr + sizeof(int);

	memcpy(ptr, &info->ny, sizeof(int));
	_size = _size + sizeof(int);
	ptr = ptr + sizeof(int);

	memcpy(ptr, &info->rot, sizeof(int));
	_size = _size + sizeof(int);
	ptr = ptr + sizeof(int);

	memcpy(ptr, &info->score, sizeof(int));				// ���̵��� ������
	_size = _size + sizeof(int);							// ������ ����
	ptr = ptr + sizeof(int);	
	
	memcpy(ptr, &info->bricknum, sizeof(int));				// ���̵��� ������
	_size = _size + sizeof(int);							// ������ ����
	ptr = ptr + sizeof(int);								// ��ġ �̵�



	memcpy(ptr, &info2->brick, sizeof(int));				// ���̵��� ������
	_size = _size + sizeof(int);							// ������ ����
	ptr = ptr + sizeof(int);								// ��ġ �̵�

	memcpy(ptr, &info2->nbrick, sizeof(int));				// ���̵��� ������
	_size = _size + sizeof(int);							// ������ ����
	ptr = ptr + sizeof(int);								// ��ġ �̵�

	memcpy(ptr, &info2->nx, sizeof(int));
	_size = _size + sizeof(int);
	ptr = ptr + sizeof(int);

	memcpy(ptr, &info2->ny, sizeof(int));
	_size = _size + sizeof(int);
	ptr = ptr + sizeof(int);

	memcpy(ptr, &info2->rot, sizeof(int));
	_size = _size + sizeof(int);

	memcpy(ptr, &info2->score, sizeof(int));				// ���̵��� ������
	_size = _size + sizeof(int);							// ������ ����
	ptr = ptr + sizeof(int);

	memcpy(ptr, &info2->bricknum, sizeof(int));				// ���̵��� ������
	_size = _size + sizeof(int);							// ������ ����
	ptr = ptr + sizeof(int);								// ��ġ �̵�

	ptr = _buf;
	memcpy(ptr, &_size, sizeof(int));

	_size = _size + sizeof(_size);
}
void PackPacket(char* _buf, PROTOCOL _protocol, RESULT _result, int& _size)
{
	char* ptr = _buf;
	_size = 0;

	ptr = ptr + sizeof(_size);

	memcpy(ptr, &_protocol, sizeof(_protocol));
	ptr = ptr + sizeof(_protocol);
	_size = _size + sizeof(_protocol);

	memcpy(ptr, &_result, sizeof(_result));
	ptr = ptr + sizeof(_result);
	_size = _size + sizeof(_result);

	ptr = _buf;
	memcpy(ptr, &_size, sizeof(_size));

	_size = _size + sizeof(_size);
}
void PackPacket(char* _buf, PROTOCOL _protocol, RESULT _result, int _count, int& _size)
{
	char* ptr = _buf;
	_size = 0;
	ptr = ptr + sizeof(_size);

	memcpy(ptr, &_protocol, sizeof(_protocol));
	ptr = ptr + sizeof(_protocol);
	_size = _size + sizeof(_protocol);

	memcpy(ptr, &_result, sizeof(_result));
	ptr = ptr + sizeof(_result);
	_size = _size + sizeof(_result);

	memcpy(ptr, &_count, sizeof(_count));
	ptr = ptr + sizeof(_count);
	_size = _size + sizeof(_count);

	ptr = _buf;
	memcpy(ptr, &_size, sizeof(_size));

	_size = _size + sizeof(_size);
}
void PackPacket(char* _buf, PROTOCOL _protocol, TETRISRESULT _result, struct _Tetris_info* _info, int& _size)
{
	char* ptr = _buf;
	_size = 0;
	ptr = ptr + sizeof(_size);

	memcpy(ptr, &_protocol, sizeof(_protocol));
	ptr = ptr + sizeof(_protocol);
	_size = _size + sizeof(_protocol);

	memcpy(ptr, &_result, sizeof(_result));
	ptr = ptr + sizeof(_result);
	_size = _size + sizeof(_result);

	memcpy(ptr, &_info->nbrick, sizeof(_info->nbrick));
	ptr = ptr + sizeof(_info->nbrick);
	_size = _size + sizeof(_info->nbrick);

	memcpy(ptr, &_info->brick, sizeof(_info->brick));
	ptr = ptr + sizeof(_info->brick);
	_size = _size + sizeof(_info->brick);

	memcpy(ptr, &_info->nx, sizeof(_info->nx));
	ptr = ptr + sizeof(_info->nx);
	_size = _size + sizeof(_info->nx);

	memcpy(ptr, &_info->ny, sizeof(_info->ny));
	ptr = ptr + sizeof(_info->ny);
	_size = _size + sizeof(_info->ny);

	memcpy(ptr, &_info->rot, sizeof(_info->rot));
	ptr = ptr + sizeof(_info->rot);
	_size = _size + sizeof(_info->rot);

	memcpy(ptr, &_info->score, sizeof(_info->score));
	ptr = ptr + sizeof(_info->score);
	_size = _size + sizeof(_info->score);

	memcpy(ptr, &_info->bricknum, sizeof(_info->bricknum));
	ptr = ptr + sizeof(_info->bricknum);
	_size = _size + sizeof(_info->bricknum);

	ptr = _buf;
	memcpy(ptr, &_size, sizeof(_size));

	_size = _size + sizeof(_size);
}
void PackPacket(char* _buf, PROTOCOL _protocol, int& _size)
{
	char* ptr = _buf;
	_size = 0;
	ptr = ptr + sizeof(_size);

	memcpy(ptr, &_protocol, sizeof(_protocol));
	ptr = ptr + sizeof(_protocol);
	_size = _size + sizeof(_protocol);

	ptr = _buf;
	memcpy(ptr, &_size, sizeof(_size));

	_size = _size + sizeof(_size);

}
void PackPacket(char* _buf, PROTOCOL _protocol, bool _set, int& _size)
{
	char* ptr = _buf;
	_size = 0;
	ptr = ptr + sizeof(_size);

	memcpy(ptr, &_protocol, sizeof(_protocol));
	ptr = ptr + sizeof(_protocol);
	_size = _size + sizeof(_protocol);


	memcpy(ptr, &_set, sizeof(bool));
	ptr = ptr + sizeof(bool);
	_size = _size + sizeof(bool);

	ptr = _buf;
	memcpy(ptr, &_size, sizeof(_size));

	_size = _size + sizeof(_size);
}
void UnPackPacket(char* _buf, char* _str1, char* _str2, char* _str3)
{
	int str1size, str2size, str3size;

	char* ptr = _buf + sizeof(PROTOCOL);

	memcpy(&str1size, ptr, sizeof(str1size));
	ptr = ptr + sizeof(str1size);

	memcpy(_str1, ptr, str1size);
	ptr = ptr + str1size;

	memcpy(&str2size, ptr, sizeof(str2size));
	ptr = ptr + sizeof(str2size);

	memcpy(_str2, ptr, str2size);
	ptr = ptr + str2size;

	memcpy(&str3size, ptr, sizeof(str3size));
	ptr = ptr + sizeof(str3size);

	memcpy(_str3, ptr, str3size);
	ptr = ptr + str3size;
}
void UnPackPacket(char* _buf, char* _str1, char* _str2)
{
	int str1size, str2size;

	char* ptr = _buf + sizeof(PROTOCOL);

	memcpy(&str1size, ptr, sizeof(str1size));
	ptr = ptr + sizeof(str1size);

	memcpy(_str1, ptr, str1size);
	ptr = ptr + str1size;

	memcpy(&str2size, ptr, sizeof(str2size));
	ptr = ptr + sizeof(str2size);

	memcpy(_str2, ptr, str2size);
	ptr = ptr + str2size;
}
void UnPackPacket(char* _buf, int& _brick)
{
	char* ptr = _buf + sizeof(PROTOCOL);
	memcpy(&_brick, ptr, sizeof(int));

}
void UnPackPacket(char* _buf, _Tetris_info* info)
{
	char* ptr = _buf + sizeof(PROTOCOL);

	memcpy(&info->nbrick, ptr, sizeof(int));
	ptr = ptr + sizeof(int);

	memcpy(&info->brick, ptr, sizeof(int));
	ptr = ptr + sizeof(int);

	memcpy(&info->nx, ptr, sizeof(int));
	ptr = ptr + sizeof(int);

	memcpy(&info->ny, ptr, sizeof(int));
	ptr = ptr + sizeof(int);

	memcpy(&info->rot, ptr, sizeof(int));
	ptr = ptr + sizeof(int);

	memcpy(&info->score, ptr, sizeof(int));
	ptr = ptr + sizeof(int);

	memcpy(&info->bricknum, ptr, sizeof(int));
	ptr = ptr + sizeof(int);

	memcpy(&info->setBrick, ptr, sizeof(bool));
	ptr = ptr + sizeof(bool);

}
void UnPackEnemyPacket(char* _buf, _Tetris_info* e_info)
{
	char* ptr = _buf + sizeof(PROTOCOL);

	memcpy(&e_info->setBrick, ptr, sizeof(bool));
}

_Client_info* AddClientInfo(SOCKET _sock, SOCKADDR_IN _addr)
{
	EnterCriticalSection(&cs);
	if (Client_Count >= WSA_MAXIMUM_WAIT_EVENTS)
	{
		return nullptr;
	}
	_Client_info* ptr = new _Client_info;
	memset(ptr, 0, sizeof(_Client_info));
	ptr->sock = _sock;
	memcpy(&ptr->addr, &_addr, sizeof(SOCKADDR_IN));
	ptr->r_sizeflag = false;
	ptr->state = MENU_SELECT_STATE;


	ptr->userinfo.state = false;
	ptr->r_overlapped.ptr = ptr;
	ptr->r_overlapped.type = IO_TYPE::IO_RECV;

	ptr->s_overlapped.ptr = ptr;
	ptr->s_overlapped.type = IO_TYPE::IO_SEND;

	Client_List[Client_Count] = ptr;
	Client_Count++;
	LeaveCriticalSection(&cs);

	printf("\n[TCP ����] Ŭ���̾�Ʈ ����: IP �ּ�=%s, ��Ʈ ��ȣ=%d\n",
		inet_ntoa(ptr->addr.sin_addr), ntohs(ptr->addr.sin_port));

	return ptr;
}
void RemoveClientInfo(_Client_info* _ptr)
{
	printf("[TCP ����] Ŭ���̾�Ʈ ����: IP �ּ�=%s, ��Ʈ ��ȣ=%d\n",
		inet_ntoa(_ptr->addr.sin_addr), ntohs(_ptr->addr.sin_port));

	EnterCriticalSection(&cs);
	for (int i = 0; i < Client_Count; i++)
	{
		if (Client_List[i] == _ptr)
		{
			closesocket(Client_List[i]->sock);

			delete Client_List[i];

			for (int j = i; j < Client_Count - 1; j++)
			{
				Client_List[j] = Client_List[j + 1];
			}

			break;
		}
	}

	Client_Count--;
	LeaveCriticalSection(&cs);

}
void RecvProcess(_Client_info* _ptr)
{
	PROTOCOL protocol;

	switch (_ptr->state)
	{
	case MENU_SELECT_STATE:

		GetProtocol(_ptr->recvbuf, protocol);

		switch (protocol)
		{
		case JOIN_INFO:
			if (!JoinProcess(_ptr))
			{
				_ptr->state = DISCONNECTED_STATE;
			}
			break;
		case LOGIN_INFO:
			if (!loginProcess(_ptr))
			{
				_ptr->state = DISCONNECTED_STATE;
			}
			break;
		case GAME_INFO:
			if (!GameInitProcess(_ptr))
			{
				_ptr->state = DISCONNECTED_STATE;
			}
			break;

		case GAME_COUNTDOWN:
			if (!CountDownProcess(_ptr))
			{
				_ptr->state = DISCONNECTED_STATE;
			}
			break;
		case PLAYING_SEND:
			if (!TetrisProcess(_ptr))
			{
				_ptr->state = DISCONNECTED_STATE;
			}
			break;

		case ENEMY_BLOCKSET:

			if (!EnemyBlockSetProcess(_ptr))
			{
				_ptr->state = DISCONNECTED_STATE;
			}

			break;
		case GAME_END:
			if (!EndgameProcess(_ptr))
			{
				_ptr->state = DISCONNECTED_STATE;
			}
			break;
		}
		break;
	}
}
void SendProcess(_Client_info* _ptr)
{

	switch (_ptr->state)
	{
	case RESULT_SEND_STATE:
		_ptr->state = MENU_SELECT_STATE;
		break;
	}
}
bool Recv(_Client_info* _ptr)
{
	int retval;
	DWORD recvbytes;
	DWORD flags = 0;

	ZeroMemory(&_ptr->r_overlapped.overlapped, sizeof(_ptr->r_overlapped.overlapped));

	_ptr->r_wsabuf.buf = _ptr->recvbuf + _ptr->comp_recvbytes;

	if (_ptr->r_sizeflag)
	{
		_ptr->r_wsabuf.len = _ptr->recvbytes - _ptr->comp_recvbytes;
	}
	else
	{
		_ptr->r_wsabuf.len = sizeof(int) - _ptr->comp_recvbytes;
	}

	retval = WSARecv(_ptr->sock, &_ptr->r_wsabuf, 1, &recvbytes,
		&flags, &_ptr->r_overlapped.overlapped, nullptr);
	if (retval == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			err_display("WSARecv()");
			//RemoveClientInfo(_ptr);
			return false;
		}
	}

	return true;
}
int CompleteRecv(_Client_info* _ptr, int _completebyte)
{

	if (!_ptr->r_sizeflag)
	{
		_ptr->comp_recvbytes += _completebyte;

		if (_ptr->comp_recvbytes == sizeof(int))
		{
			memcpy(&_ptr->recvbytes, _ptr->recvbuf, sizeof(int));
			_ptr->comp_recvbytes = 0;
			_ptr->r_sizeflag = true;
		}

		if (!Recv(_ptr))
		{
			return SOC_ERROR;
		}

		return SOC_FALSE;
	}

	_ptr->comp_recvbytes += _completebyte;

	if (_ptr->comp_recvbytes == _ptr->recvbytes)
	{
		_ptr->comp_recvbytes = 0;
		_ptr->recvbytes = 0;
		_ptr->r_sizeflag = false;

		return SOC_TRUE;
	}
	else
	{
		if (!Recv(_ptr))
		{
			return SOC_ERROR;
		}

		return SOC_FALSE;
	}
}
bool Send(_Client_info* _ptr, int _size)
{
	int retval;
	DWORD sendbytes;
	ZeroMemory(&_ptr->s_overlapped.overlapped, sizeof(_ptr->s_overlapped.overlapped));
	if (_ptr->sendbytes == 0)
	{
		_ptr->sendbytes = _size;
	}

	_ptr->s_wsabuf.buf = _ptr->sendbuf + _ptr->comp_sendbytes;
	_ptr->s_wsabuf.len = _ptr->sendbytes - _ptr->comp_sendbytes;

	retval = WSASend(_ptr->sock, &_ptr->s_wsabuf, 1, &sendbytes,
		0, &_ptr->s_overlapped.overlapped, nullptr);
	if (retval == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			err_display("WSASend()");
			//RemoveClientInfo(_ptr);
			return false;
		}
	}

	return true;
}
int CompleteSend(_Client_info* _ptr, int _completebyte)
{
	_ptr->comp_sendbytes += _completebyte;
	if (_ptr->comp_sendbytes == _ptr->sendbytes)
	{
		_ptr->comp_sendbytes = 0;
		_ptr->sendbytes = 0;

		return SOC_TRUE;
	}
	if (!Send(_ptr, _ptr->sendbytes))
	{
		return SOC_ERROR;
	}

	return SOC_FALSE;
}
bool JoinProcess(_Client_info* _ptr)
{
	RESULT join_result = NODATA;
	char msg[BUFSIZE];
	PROTOCOL protocol;
	int size;

	memset(&_ptr->userinfo, 0, sizeof(_User_Info));
	UnPackPacket(_ptr->recvbuf, _ptr->userinfo.id, _ptr->userinfo.pw, _ptr->userinfo.nickname);
	for (int i = 0; i < Join_Count; i++)
	{
		if (!strcmp(Join_List[i]->id, _ptr->userinfo.id))
		{
			join_result = ID_EXIST;
			strcpy(msg, ID_EXIST_MSG);
			break;
		}
	}

	if (join_result == NODATA)
	{
		_User_Info* user = new _User_Info;
		memset(user, 0, sizeof(_User_Info));
		strcpy(user->id, _ptr->userinfo.id);
		strcpy(user->pw, _ptr->userinfo.pw);
		strcpy(user->nickname, _ptr->userinfo.nickname);

		//FileDataAdd(user);

		Join_List[Join_Count++] = user;
		join_result = JOIN_SUCCESS;
		strcpy(msg, JOIN_SUCCESS_MSG);
	}

	protocol = JOIN_RESULT;

	PackPacket(_ptr->sendbuf, protocol, join_result, msg, size);

	_ptr->state = RESULT_SEND_STATE;

	if (!Send(_ptr, size))
	{
		return false;
	}

	return true;
}
bool loginProcess(_Client_info* _ptr)
{
	RESULT login_result = NODATA;

	char msg[BUFSIZE];
	PROTOCOL protocol;
	int size;

	memset(&_ptr->userinfo, 0, sizeof(_User_Info));
	UnPackPacket(_ptr->recvbuf, _ptr->userinfo.id, _ptr->userinfo.pw);
	for (int i = 0; i < Join_Count; i++)
	{
		if (!strcmp(Join_List[i]->id, _ptr->userinfo.id))
		{
			if (!strcmp(Join_List[i]->pw, _ptr->userinfo.pw))
			{
				login_result = LOGIN_SUCCESS;
				strcpy(msg, LOGIN_SUCCESS_MSG);
				_ptr->userinfo.state = true;
				strcpy(_ptr->userinfo.nickname, Join_List[i]->nickname);
			}
			else
			{
				login_result = PW_ERROR;
				strcpy(msg, PW_ERROR_MSG);
			}
			break;
		}
	}

	if (login_result == NODATA)
	{
		login_result = ID_ERROR;
		strcpy(msg, ID_ERROR_MSG);
	}

	protocol = LOGIN_RESULT;

	PackPacket(_ptr->sendbuf, protocol, login_result, msg, size);

	_ptr->state = RESULT_SEND_STATE;

	if (!Send(_ptr, size))
	{
		return false;
	}
	return true;
}
bool GameInitProcess(_Client_info* _ptr)
{
	PROTOCOL protocol;
	int size;
	roomptr = SearchEmptyRoom(_ptr);
	Add_User(_ptr, roomptr);

	protocol = PROTOCOL::GAME_INIT;
	RESULT result;

	// �濡 2���� �� �� ���
	if (roomptr->r_state == ROOM_STATE::ROOM_PLAY) {
		result = RESULT::MATCHING_SUCCESS;

		// ����ü ����
		roomptr->users[0]->gameInfo = new _Tetris_info();
		roomptr->users[1]->gameInfo = new _Tetris_info();


		// ���� ����, ����, ���� ���� �ʱ�ȸ
		MakeBrick(roomptr->users[0]->gameInfo);
		MakeBrick(roomptr->users[1]->gameInfo);
		roomptr->users[0]->gameInfo->score = 0;
		roomptr->users[0]->gameInfo->bricknum = 0;
		roomptr->users[1]->gameInfo->score = 0;
		roomptr->users[1]->gameInfo->bricknum = 0;

		// ������ �ʱ�ȭ
		BoardInit(roomptr);

		protocol = PROTOCOL::GAME_INIT;

		// �������, �������� ���� ����ڿ��� �ش�. �� ������ ī��Ʈ�ٿ��� ���۵ȴ�.

		PackPacket(roomptr->users[0]->sendbuf, protocol, result, roomptr->users[1]->userinfo.nickname, 
			roomptr->users[0]->gameInfo, roomptr->users[1]->gameInfo, size);
		PackPacket(roomptr->users[1]->sendbuf, protocol, result, roomptr->users[0]->userinfo.nickname, 
			roomptr->users[1]->gameInfo, roomptr->users[0]->gameInfo, size);

		roomptr->users[0]->state = RESULT_SEND_STATE;
		roomptr->users[1]->state = RESULT_SEND_STATE;

		if (!Send(roomptr->users[1], size))
		{
			return false;
		}

		if (!Send(roomptr->users[0], size))
		{
			return false;
		}
	}

	// �� �� ��쿡�� ��Ī ��⸦ ������.
	else {
		result = RESULT::MATCHING_WAIT;
		protocol = PROTOCOL::GAME_INIT;
		PackPacket(_ptr->sendbuf, protocol, result, size);

		_ptr->state = RESULT_SEND_STATE;

		if (!Send(_ptr, size))
		{
			return false;
		}
	}

	return true;
}
bool CountDownProcess(_Client_info* _ptr)
{
	PROTOCOL protocol;
	int size;
	RESULT result;

	for (int i = 0; i < room_count; i++)
	{
		for (int j = 0; j < 2; j++)
		{
			if (_ptr == RoomInfo[i]->users[j])
			{
				// ���� ������ �Ȱ��� ī��Ʈ�� �ٵ��� �Ѵ�.
				if (RoomInfo[i]->users[0]->gamesend && RoomInfo[i]->users[1]->gamesend)
				{
					RoomInfo[i]->users[0]->gamesend = false;
					RoomInfo[i]->users[1]->gamesend = false;
					RoomInfo[i]->count++;
				}

				_ptr->gamesend = true;
				protocol = PROTOCOL::GAME_COUNTDOWN;
				// 3�� ī��Ʈ����
				if (COUNT - (RoomInfo[i]->count) > 0)
				{
					printf("%d\n", COUNT - (RoomInfo[i]->count));
					result = RESULT::COUNTING;
					PackPacket(_ptr->sendbuf, protocol, result, COUNT - (RoomInfo[i]->count), size);
				}

				// �� ���� ��쿡�� START ���
				else {
					printf("START!\n");
					result = RESULT::COUNTING_END;
					PackPacket(_ptr->sendbuf, protocol, result, "START!!", size);
				}

				_ptr->state = RESULT_SEND_STATE;

				if (!Send(_ptr, size))
				{
					return false;
				}

				break;
			}
		}
	}

	return true;
}
bool TetrisProcess(_Client_info* _ptr)
{
	PROTOCOL protocol;
	TETRISRESULT result;
	int size;

	for (int i = 0; i < room_count; i++)
	{
		for (int j = 0; j < 2; j++)
		{
			if (_ptr == RoomInfo[i]->users[j])
			{

				protocol = PROTOCOL::PLAYING_RECV;

				// ���� �ƴ� ���� ���� ����
				if (j == 0) {
					UnPackPacket(_ptr->recvbuf, RoomInfo[i]->users[0]->gameInfo);
					result = TETRISRESULT::NORMAL;
					PackPacket(RoomInfo[i]->users[1]->sendbuf, protocol, result, RoomInfo[i]->users[0]->gameInfo, size);
					// ��Ŷ �����ֱ�

					RoomInfo[i]->users[1]->state = RESULT_SEND_STATE;

					if (!Send(RoomInfo[i]->users[1], size))
					{
						return false;
					}

					break;
				}

				// ���� �ƴ� ���� ���� ����
				else
				{
					UnPackPacket(_ptr->recvbuf, RoomInfo[i]->users[1]->gameInfo);
					result = TETRISRESULT::NORMAL;
					PackPacket(RoomInfo[i]->users[0]->sendbuf, protocol, result, RoomInfo[i]->users[1]->gameInfo, size);
					// ��Ŷ �����ֱ�

					RoomInfo[i]->users[0]->state = RESULT_SEND_STATE;

					if (!Send(RoomInfo[i]->users[0], size))
					{
						return false;
					}

					break;
				}
			}
		}
	}

	return true;
}
bool EnemyBlockSetProcess(_Client_info* _ptr)
{
	PROTOCOL protocol;
	int size;

	for (int i = 0; i < room_count; i++)
	{
		for (int j = 0; j < 2; j++)
		{
			if (_ptr == RoomInfo[i]->users[j])
			{
				protocol = PROTOCOL::ENEMY_BLOCKSET;

				if (j == 0)
				{
					// ����� ��������� �޾� ��뿡�� ������ ������ �����ش�.
					UnPackEnemyPacket(_ptr->recvbuf, RoomInfo[i]->users[1]->gameInfo);
					PackPacket(RoomInfo[i]->users[1]->sendbuf, protocol, RoomInfo[i]->users[1]->gameInfo->setBrick, size);
					RoomInfo[i]->users[1]->state = RESULT_SEND_STATE;
					if (!Send(RoomInfo[i]->users[1], size))
					{
						return false;
					}

					break;
				}

				else
				{
					// ����� ��������� �޾� ��뿡�� ������ ������ �����ش�.
					UnPackEnemyPacket(_ptr->recvbuf, RoomInfo[i]->users[0]->gameInfo);
					PackPacket(RoomInfo[i]->users[0]->sendbuf, protocol, RoomInfo[i]->users[0]->gameInfo->setBrick, size);
					RoomInfo[i]->users[0]->state = RESULT_SEND_STATE;

					if (!Send(RoomInfo[i]->users[0], size))
					{
						return false;
					}
					break;
				}
			}
		}
	}


	return true;
}


bool EndgameProcess(_Client_info* _ptr)
{
	int size;
	bool flag = false;

	for (int i = 0; i < room_count; i++)
	{
		for (int j = 0; j < 2; j++) {
			if (_ptr == RoomInfo[i]->users[j])
			{
				if (j == 0)
				{
					PackPacket(RoomInfo[i]->users[1]->sendbuf, PROTOCOL::GAME_END, size);

					RoomInfo[i]->users[1]->state = RESULT_SEND_STATE;

					if (!Send(RoomInfo[i]->users[1], size))
					{
						return false;
					}
				}
				else
				{
					PackPacket(RoomInfo[i]->users[0]->sendbuf, PROTOCOL::GAME_END, size);

					RoomInfo[i]->users[0]->state = RESULT_SEND_STATE;

					if (!Send(RoomInfo[i]->users[0], size))
					{
						return false;
					}
				}
				
				flag = true;
			}

			if (flag) break;
		}

		if (flag) break;
	}


	return true;
}

_Room_Info* AddRoomInfo()
{
	EnterCriticalSection(&cs);
	_Room_Info* ptr = new _Room_Info;
	ZeroMemory(ptr, sizeof(_Room_Info));

	// ���� ���� ����
	ptr->r_state = ROOM_WAIT;
	// �÷��̾� ���� ����
	ZeroMemory(ptr->users, sizeof(_Client_info*) * 2);


	RoomInfo[room_count++] = ptr;

	printf("\n����Ʈ�� �� �߰�\n");

	LeaveCriticalSection(&cs);
	return ptr;
}
void RemoveRoomInfo(_Room_Info* _ptr)
{
	printf("\n����Ʈ���� �� ����\n");

	EnterCriticalSection(&cs); // �迭 �����ؾ��ϴϱ� ġ������ ���� �� �����
	for (int i = 0; i < room_count; i++)
	{
		if (RoomInfo[i] == _ptr) // �ش��ϴ� �� ã�Ҵٸ�
		{
			// ���� �����
			delete RoomInfo[i];

			// �迭 �����
			for (int j = i; j < room_count - 1; j++)
				RoomInfo[j] = RoomInfo[j + 1];

			break; // �� �� �� �����ϱ� �ݺ��� Ż��
		}
	}
	room_count--;

	LeaveCriticalSection(&cs); // �� �� ���
}
_Room_Info* SearchEmptyRoom(_Client_info* _client)
{
	for (int i = 0; i < room_count; i++) // ���� ��ϵ� �� �߿���
	{
		if (RoomInfo[i]->r_state == ROOM_WAIT) // �׳� ��� �ִ� ���� �ְ�
		{
			for (int j = 0; j < 2; j++) // Ŭ�� ����� ��������
			{
				if (RoomInfo[i]->users[j] != nullptr)
				{
					if (RoomInfo[i]->users[j] == _client) // �ڱ� �ڽ��� �ִ� ���
						break; // �� ���� �ƴϿ�
				}
				if (RoomInfo[i]->users[j] == nullptr) // �ű⿡ ���� �� �ڸ� ������
					return RoomInfo[i]; // �� ���� �ּ����ش�
			}
		}
	}

	// �ƹ� �浵 ���ų� ���� �� �� ���ְų� �� ��� ������� �´�
	return AddRoomInfo(); // ���� �� ���� �װ� �ּ����ش�
}
bool Add_User(_Client_info* _data, _Room_Info* room)
{
	for (int i = 0; i < 2; i++) // �� �ڸ� ã�� �ݺ���
	{
		if (room->users[i] == nullptr) // �� �ڸ� �߰�
		{
			room->users[i] = _data; // �� �ڸ��� ���� �� Ŭ���� �ڸ��� �Ѵ�
			printf("%s ���� %d ���濡 ���Խ��ϴ�.\n", _data->userinfo.nickname, room_count - 1);
			if (i == 1) // ��� �ο��� �𿴴ٸ�
			{
				room->r_state = ROOM_PLAY;	// �� ���� ��ȯ
				printf("%d�� �濡 ����� �𿴴�!!\n", room_count - 1);
			}
			return true;
		}
	}
	// ���� ���� �ϳ� �� ã������ false
	return false;
}
void Remove_User(_Client_info* _data)
{
	_data->state = STATE::DISCONNECTED_STATE;

	// �����
	for (int i = 0; i < 2; i++)
	{
		if (room_count > 0 && RoomInfo[room_count - 1]->users[i] == _data)
		{
			printf("%s �� ��Ż��!\n", _data->userinfo.nickname);
			// �迭 �����
			for (int j = i; j < 1; j++)
			{
				if (RoomInfo[room_count - 1]->users[j + 1] == nullptr) {
					printf("�濡 �ٸ� �����ڰ� ���� ����� %d�� ���� ��������ϴ�.\n", room_count - 1);
					room_count--;
					return;
				}
				RoomInfo[room_count - 1]->users[j] = RoomInfo[room_count - 1]->users[j + 1];
				RoomInfo[room_count - 1]->users[j + 1] = nullptr;
			}
			break;
		}
	}

	// ���� ���� ���� ��
	if (room_count > 0 && RoomInfo[room_count - 1]->r_state == ROOM_PLAY)
	{
		if (RoomInfo[room_count - 1]->users[0] == nullptr || RoomInfo[room_count - 1]->users[1] == nullptr)
			RoomInfo[room_count - 1]->r_state = ROOM_END;
	}

	// ��� ��ٸ��� �ߵ� �ƴѵ� �濡 �ƹ��� ���� ���
	if (room_count > 0 && RoomInfo[room_count - 1]->users[0] == nullptr && RoomInfo[room_count - 1]->r_state != ROOM_WAIT)
		RemoveRoomInfo(RoomInfo[room_count - 1]); // �����θ� ����
}
void BoardInit(_Room_Info* _ptr)
{

	for (int x = 0; x < BW + 2; x++)
	{
		for (int y = 0; y < BH + 2; y++)
		{
			_ptr->users[0]->gameInfo->board[x][y] = (y == 0 || y == BH + 1 || x == 0 || x == BW + 1) ? WALL : EMPTY;
			_ptr->users[1]->gameInfo->board[x][y] = (y == 0 || y == BH + 1 || x == 0 || x == BW + 1) ? WALL : EMPTY;
		}
	}

}
void MakeBrick(_Tetris_info* info)
{
	int bw = BW;

	info->brick = random(sizeof(Shape) / sizeof(Shape[0]));
	info->nbrick = random(sizeof(Shape) / sizeof(Shape[0]));
	info->nx = bw / 2;
	info->ny = 3;
	info->rot = 0;
}