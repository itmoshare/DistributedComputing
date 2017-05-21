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

static void new_message(Message *msg, MessageType type)
{
	memset(msg, 0, sizeof(Message));
	msg->s_header.s_magic = MESSAGE_MAGIC;
	msg->s_header.s_type = type;
	msg->s_header.s_local_time = get_lamport_time();
}
#endif
