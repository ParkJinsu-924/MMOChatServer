#include "MMOChatServer.h"
#include <conio.h>
MMOChatServer myServer;
extern int debugIndex;
CCrashDump dump;


int main()
{
	if (!myServer.Start(L"0.0.0.0", 12001, 3, 3, false, 20000))
	{
		printf("서버가동실패\n");
	}

	for (;;)
	{
		if (_kbhit())
		{
			_getch();
			printf(">>> AcceptCount %lli\n", myServer.GetAcceptCount());
			printf(">>> LogIndex %d\n", myServer.logIndex);
			/*printf("AcceptCount %lli\n", myServer.GetAcceptCount());
			printf("CPacketPoolCount : %d\n", CSerializationBuffer::GetMemoryPoolAllocCount());
			printf("CJobPoolCount : %d\n", CJob::cJobMemoryPool.GetAllocCount());
			printf("CPlayerCount : %d\n", Player::playerMemoryPool.GetAllocCount());
			printf("MemoryPoolAllocCount %d\n", debugIndex);
			printf("ClientNum = %d\n\n", myServer.GetClientCount());*/

		}
	}

	return 1;
}