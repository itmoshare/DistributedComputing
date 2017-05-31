#ifndef _EXTRA
#define _EXTRA
#include "banking.h"

typedef struct 
{
	int readEnd, writeEnd;
}__attribute__((packed)) Pipe;

typedef struct 
{
	local_id local_pid;
	Pipe **pipes;
	size_t proc_ct;
	BalanceHistory *history;
} ProcInfo;

void new_message(Message *msg, MessageType type);

#endif
