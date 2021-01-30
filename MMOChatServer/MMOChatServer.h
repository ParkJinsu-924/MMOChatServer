#pragma once
#include "CNetServer\CNetServer.h"
#include "CLockFreeQueue\LockFreeQueue.h"
#include "CCrashDump/CCrashDump.h"
#include "Structs.h"
#include "CommonProtocol.h"
#include <locale.h>

using namespace std;
std::set<UINT64> setset;
class CJob
{
	friend class MMOChatServer;
private:
	UINT64 sessionID;
	char handlerType;
	CSerializationBuffer* pPacket;

public: static CMemoryPoolTLS<CJob> cJobMemoryPool;

public:
	static CJob* Alloc()
	{
		return cJobMemoryPool.Alloc();
	}

	static bool Free(CJob* pJob)
	{
		return cJobMemoryPool.Free(pJob);
	}
};

CMemoryPoolTLS<CJob> CJob::cJobMemoryPool;

class Player
{
	friend class MMOChatServer;
private:
	UINT64 sessionID;
	INT64 accoutNo;
	WCHAR ID[20];
	WCHAR nickName[20];
	char sessionKey[64];
	stSECTOR_POS playerSector;

public: static CMemoryPoolTLS<Player> playerMemoryPool;

public:
	static Player* Alloc()
	{
		return playerMemoryPool.Alloc();
	}

	static bool Free(Player* pPlayer)
	{
		return playerMemoryPool.Free(pPlayer);
	}
};

CMemoryPoolTLS<Player> Player::playerMemoryPool;

#define X 50 // 벌점 10점
#define Y 50

class MMOChatServer : public CNetServer
{
public:
	MMOChatServer()
	{
		hEvent = CreateEvent(NULL, false, false, NULL);//벌점!!
		_beginthreadex(NULL, NULL, StaticUpdate, (PVOID)this, NULL, NULL);
		//_beginthreadex(NULL, NULL, StaticLog, (PVOID)this, NULL, NULL); // ? 아항 Main private? public? ㅇㅉ 
	}

private:
	enum { eJOIN, eRECV, eLEAVE };

	void OnClientJoin(UINT64 sessionID) override // 상점 ! // final
	{
		CJob* pJob = CreateCJob(sessionID, eJOIN);
		updateThreadQueue.Enqueue(pJob);
		SetEvent(hEvent);
	}

	void OnClientLeave(UINT64 sessionID) override
	{
		CJob* pJob = CreateCJob(sessionID, eLEAVE);
		updateThreadQueue.Enqueue(pJob);
		SetEvent(hEvent);
	}

	bool OnConnectionRequest(PWCHAR, short) override
	{
		//TODO
		return true;
	}

	VOID OnRecv(UINT64 sessionID, CSerializationBuffer* buf) override
	{
		buf->AddRef();

		CJob* pJob = CreateCJob(sessionID, eRECV, buf);
		updateThreadQueue.Enqueue(pJob);
		SetEvent(hEvent);
	}

	VOID OnError() override
	{
		// TODO
	}

	static unsigned WINAPI StaticUpdate(PVOID p)
	{
		MMOChatServer* chatServer = (MMOChatServer*)p;

		chatServer->Update();

		return 1;
	}

	static unsigned WINAPI StaticLog(PVOID p)
	{
		MMOChatServer* chatServer = (MMOChatServer*)p;

		chatServer->Log();

		return 1;
	}

	void Log()
	{
		for (;;)
		{
			printf("SIZE ==> %d\n", characterMap.size());
			printf("PLAY allocCount %d\n", Player::playerMemoryPool.GetAllocCount());
			printf("CJOB allocCount %d\n", CJob::cJobMemoryPool.GetAllocCount());
			//printf("PACK allocCount %d\n\n", CSerializationBuffer::memoryPool.GetAllocCount());
			Sleep(1000);
		}
	}

	void Update()
	{
		for (;;)
		{
			WaitForSingleObject(hEvent, INFINITE);

			for (;;)
			{
				CJob* pJob;
				if (!updateThreadQueue.Dequeue(&pJob))
				{
					break;
				}

				switch (pJob->handlerType)
				{
				case eJOIN:
					JoinProc(pJob);
					break;
				case eRECV:
					RecvProc(pJob);
					break;
				case eLEAVE:
					LeaveProc(pJob);
					break;
				default:
					wprintf(L"이상합니다.\n");
					//강력한 로그!
					break;
				}
				CJob::Free(pJob);
			}
		}
	}

	void JoinProc(CJob* pJob)
	{
		// TODO
	}

	void RecvProc(CJob* pJob)
	{
		CSerializationBuffer* pPacket = pJob->pPacket;
		WORD type;

		pPacket->GetContentData((char*)&type, sizeof(type));

		switch (type)
		{
		case en_PACKET_CS_CHAT_REQ_LOGIN:
			ReqLoginProc(pJob);
			break;
		case en_PACKET_CS_CHAT_REQ_SECTOR_MOVE:
			ReqSectorMove(pJob);
			break;
		case en_PACKET_CS_CHAT_REQ_MESSAGE:
			ReqMessage(pJob);
			break;
		case en_PACKET_CS_CHAT_REQ_HEARTBEAT:
			break;
		default:
			Disconnect(pJob->sessionID);
			break;
		}

		//새로 alloc받은 직렬화 버퍼이기 때문에 DeqRef를 해주어야한다.
		pPacket->DeqRef();
	}

	void LeaveProc(CJob* pJob)
	{
		Player* pPlayer = nullptr;

		auto itor = characterMap.find(pJob->sessionID);

		if (itor != characterMap.end())
		{
			pPlayer = itor->second;

			characterMap.erase(pPlayer->sessionID);

			if (pPlayer->playerSector.iY != -1 && pPlayer->playerSector.iX != -1)
				sectorMap[pPlayer->playerSector.iY][pPlayer->playerSector.iX].erase(pPlayer->sessionID);

			Player::Free(pPlayer);
		}
		else
		{
			logIndex++;
		}
	}

	void ReqLoginProc(CJob* pJob)
	{
		auto itor = characterMap.find(pJob->sessionID);

		if (itor != characterMap.end()) // 중복로긴 없음
		{
			Disconnect(pJob->sessionID);
			return;
		}

		CSerializationBuffer* pPacket = pJob->pPacket;

		if (pPacket->GetContentUseSize() < sizeof(INT64) + sizeof(WCHAR) * 20 + sizeof(WCHAR) * 20 + sizeof(char) * 64)
		{
			Disconnect(pJob->sessionID);
			return;
		}

		Player* newPlayer = Player::Alloc();
		newPlayer->sessionID = pJob->sessionID;


		pPacket->GetContentData((char*)&newPlayer->accoutNo, sizeof(newPlayer->accoutNo)); // 상점 20점
		pPacket->GetContentData((char*)newPlayer->ID, sizeof(newPlayer->ID));
		pPacket->GetContentData((char*)newPlayer->nickName, sizeof(newPlayer->nickName));
		pPacket->GetContentData((char*)newPlayer->sessionKey, sizeof(newPlayer->sessionKey)); 

		if (!PacketEmptyCheck(pPacket))
		{
			Disconnect(pJob->sessionID);
			return;
		}

		newPlayer->playerSector.iX = -1;// Rand() 레게노
		newPlayer->playerSector.iY = -1;

		characterMap.insert(make_pair(pJob->sessionID, newPlayer));

		CSerializationBuffer* resPacket = CSerializationBuffer::Alloc();

		WORD type = en_PACKET_CS_CHAT_RES_LOGIN;
		BYTE status = 1;
		INT64 accountNo = newPlayer->accoutNo;

		resPacket->PutContentData((char*)&type, sizeof(type));
		resPacket->PutContentData((char*)&status, sizeof(status));
		resPacket->PutContentData((char*)&accountNo, sizeof(accountNo));

		SendPacket(newPlayer->sessionID, resPacket);

		resPacket->DeqRef();
	}

	void ReqSectorMove(CJob* pJob)
	{
		INT64 accountNo;
		WORD sectorX;
		WORD sectorY;

		CSerializationBuffer* pPacket = pJob->pPacket;

		if (pPacket->GetContentUseSize() < sizeof(accountNo) + sizeof(sectorX) + sizeof(sectorY))
		{
			Disconnect(pJob->sessionID);
			return;
		}

		pPacket->GetContentData((char*)&accountNo, sizeof(accountNo));
		pPacket->GetContentData((char*)&sectorX, sizeof(sectorX));
		pPacket->GetContentData((char*)&sectorY, sizeof(sectorY));

		if (sectorX >= X || sectorY >= Y)
		{
			Disconnect(pJob->sessionID);
			return;
		}

		auto characterItor = characterMap.find(pJob->sessionID);

		if (characterItor == characterMap.end())
		{
			printf("ReqSectorMove_Disconnect!\n");
			Disconnect(pJob->sessionID);
			return;
		}

		Player* pPlayer = characterItor->second;

		if (pPlayer->accoutNo != accountNo || PacketEmptyCheck(pPacket) == false)
		{
			Disconnect(pJob->sessionID);
			return;
		}

		if (pPlayer->playerSector.iX != -1 && pPlayer->playerSector.iY != -1)
		{
			sectorMap[pPlayer->playerSector.iY][pPlayer->playerSector.iX].erase(pPlayer->sessionID);
		}

		sectorMap[sectorY][sectorX].insert(make_pair(pPlayer->sessionID, pPlayer));

		pPlayer->playerSector.iX = sectorX;
		pPlayer->playerSector.iY = sectorY;

		CSerializationBuffer* resPacket = CSerializationBuffer::Alloc();

		WORD type = en_PACKET_CS_CHAT_RES_SECTOR_MOVE;
		accountNo = pPlayer->accoutNo;
		sectorX = pPlayer->playerSector.iX;
		sectorY = pPlayer->playerSector.iY;

		resPacket->PutContentData((char*)&type, sizeof(type));
		resPacket->PutContentData((char*)&accountNo, sizeof(accountNo));
		resPacket->PutContentData((char*)&sectorX, sizeof(sectorX));
		resPacket->PutContentData((char*)&sectorY, sizeof(sectorY));

		SendPacket(pPlayer->sessionID, resPacket);

		resPacket->DeqRef();
	}

	void ReqMessage(CJob* pJob)
	{
		INT64 accountNo;
		WORD messageLen;

		CSerializationBuffer* pPacket = pJob->pPacket;

		if (pPacket->GetContentUseSize() < sizeof(accountNo) + sizeof(messageLen))
		{
			Disconnect(pJob->sessionID);
			return;
		}

		auto characterItor = characterMap.find(pJob->sessionID);

		if (characterItor == characterMap.end())
		{
			printf("ReqMessage_Disconnect!\n");
			Disconnect(pJob->sessionID);
			return;
		}

		pPacket->GetContentData((char*)&accountNo, sizeof(accountNo));
		pPacket->GetContentData((char*)&messageLen, sizeof(messageLen));

		Player* pPlayer = characterItor->second;

		if (pPlayer->accoutNo != accountNo || messageLen >= 400) 
		{
			Disconnect(pJob->sessionID);
			return;
		}

		WCHAR pMessage[200];

		if (pPacket->GetContentUseSize() != messageLen)
		{
			Disconnect(pJob->sessionID);
			return;
		}

		pPacket->GetContentData((char*)pMessage, messageLen);

		if (!PacketEmptyCheck(pPacket))
		{
			Disconnect(pJob->sessionID);
			return;
		}

		CSerializationBuffer* resPacket = CSerializationBuffer::Alloc();

		WORD type = en_PACKET_CS_CHAT_RES_MESSAGE;
		resPacket->PutContentData((char*)&type, sizeof(type));
		resPacket->PutContentData((char*)&pPlayer->accoutNo, sizeof(pPlayer->accoutNo));
		resPacket->PutContentData((char*)pPlayer->ID, sizeof(pPlayer->ID));
		resPacket->PutContentData((char*)pPlayer->nickName, sizeof(pPlayer->nickName));

		resPacket->PutContentData((char*)&messageLen, sizeof(messageLen));
		resPacket->PutContentData((char*)pMessage, messageLen);

		//SendPacket(pPlayer->sessionID, resPacket);
		SendPacketAround(pPlayer, resPacket);

		resPacket->DeqRef();
	}

	CJob* CreateCJob(UINT64 sessionID, char handlerType, CSerializationBuffer* pPacket = nullptr)
	{
		CJob* pJob = CJob::Alloc();
		pJob->sessionID = sessionID;
		pJob->handlerType = handlerType;
		pJob->pPacket = pPacket;
		return pJob;
	}

	bool PacketEmptyCheck(CSerializationBuffer* pPacket)
	{
		if (pPacket->GetContentUseSize() == 0)
			return true;

		return false;
	}

	void GetSectorAround(short iSectorX, short iSectorY, stSECTOR_AROUND* pSectorAround)
	{
		//이걸 해주는 이유는 내가 현재 있는 섹터에서 좌측상단의 맨 처음 시작하는 섹터의 좌표를 얻기 위함.
		iSectorX--;
		iSectorY--;

		pSectorAround->iCount = 0;

		for (int iCntY = 0; iCntY < 3; iCntY++)
		{
			//각 X,Y의 섹터갯수는 25개, 가장자리에 있을경우 맵을 벗어나는 섹터들은 안하기위함.
			if (iSectorY + iCntY < 0 || iSectorY + iCntY >= Y)
				continue;

			for (int iCntX = 0; iCntX < 3; iCntX++)
			{
				if (iSectorX + iCntX < 0 || iSectorX + iCntX >= X)
					continue;

				pSectorAround->Around[pSectorAround->iCount].iX = iSectorX + iCntX;
				pSectorAround->Around[pSectorAround->iCount].iY = iSectorY + iCntY;
				//iCount는 자신의 영향권에 있는 섹터의 갯수
				++pSectorAround->iCount;
			}
		}
	}

	void SendPacketAround(Player* pPlayer, CSerializationBuffer* pPacket)
	{
		stSECTOR_AROUND sectorAround;

		GetSectorAround(pPlayer->playerSector.iX, pPlayer->playerSector.iY, &sectorAround);

		for (int i = 0; i < sectorAround.iCount; i++)
		{
			SendPacketOneSector(sectorAround.Around[i].iX, sectorAround.Around[i].iY, pPacket, NULL);
		}
	}

	void SendPacketOneSector(short sectorX, short sectorY, CSerializationBuffer* pPacket, Player* pExceptPlayer)
	{
		auto endIter = sectorMap[sectorY][sectorX].end();

		for (auto i = sectorMap[sectorY][sectorX].begin(); i != endIter; ++i)
		{
			//if (pExceptPlayer == i->second)
				//continue;

			SendPacket(i->first, pPacket);
		}
	}

	HANDLE hEvent;
	unordered_map<UINT64, Player*> characterMap;
	unordered_map<UINT64, Player*> sectorMap[X][Y];
	LockFreeQueue<CJob*> updateThreadQueue;
	public: UINT64 logIndex;
};

