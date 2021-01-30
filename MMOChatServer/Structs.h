#pragma once
#include <Windows.h>

struct stSECTOR_POS
{
	short iX;
	short iY;
};

//struct Player
//{
//	UINT64 sessionID;
//	INT64 accoutNo;
//	WCHAR ID[20];
//	WCHAR nickName[20];
//	char sessionKey[64];
//	stSECTOR_POS playerSector;
//};

struct stSECTOR_AROUND
{
	int iCount;
	stSECTOR_POS Around[9];
};