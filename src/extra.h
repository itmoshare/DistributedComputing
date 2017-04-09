#ifndef _EXTRA
#define _EXTRA

typedef struct 
{
	int readEnd, writeEnd;
}__attribute__((packed)) Pipe;

typedef struct 
{
	local_id pid;
	Pipe **pipes;
	size_t proc_ct;
} ProcInfo;

#endif
