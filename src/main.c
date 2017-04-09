#define _GNU_SOURCE
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>

#include <string.h>
#include <assert.h>

#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#include "common.h"
#include "ipc.h"
#include "pa1.h"

#include "extra.h"

// Close unused pipes
void closePipes(ProcInfo *proc_info)
{
	for (int i = 0; i < proc_info->proc_ct; i++)
	{
		for (int j = 0; j < proc_info->proc_ct; j++)
		{
			if (i == j) continue;
			if (i != proc_info->pid)
			{
				close(proc_info->pipes[i][j].writeEnd);
			}
			if (j != proc_info->pid)
			{
				close(proc_info->pipes[i][j].readEnd);
			}
		}
	}
}

int receiveAll(ProcInfo *proc_info)
{
	Message msg;
	for(local_id i = 1; i < proc_info->proc_ct; i++)
	{
		int rc = receive(proc_info, i, &msg);
		if( rc != 0 ) return 1;
	}
	return 0;
}

int parentAction(ProcInfo *proc_info)
{
	closePipes(proc_info);
	int rc = 0;
	rc = receiveAll(proc_info);
	if( rc != 0 ) return 1;

	rc = receiveAll(proc_info);
	if( rc != 0 ) return 1;

	for(local_id pid = 1; pid < proc_info->proc_ct; pid++)
	{
		wait(NULL);
	}
	return 0;
}

int main(int argc, char * const argv[])
{
	if (argc < 2) 
	{
		fprintf(stderr, "Not enough arguments\n");
        return 1;
    }
	
	int n = 0;
	switch (getopt(argc, argv, "p:")) 
	{
		case 'p':
			n = atoi(optarg);
			if (!n || n <= 0 || n > 10) 
			{
				fprintf(stderr, "p parameter must be int from 1 to 10\n");
				return 1;
			}
			break;
		case -1:
			return 1;
	}
	n++;
	
	ProcInfo proc_info;
	memset(&proc_info, 0, sizeof(proc_info));
	proc_info.proc_ct = n + 1;
	
	proc_info.pipes = malloc(n * sizeof(proc_info.pipes));
	for (int i = 0; i < n; i++)
		proc_info.pipes[i] = malloc(n * sizeof(Pipe));
	if (proc_info.pipes == NULL) return 1;
	
	// Open pipes
	FILE * pipes_log_f = fopen(pipes_log, "a");
	if (pipes_log_f == NULL) return 1;
	setbuf(pipes_log_f, NULL);
	
	for (int i = 0; i < n; i++) 
	{
		for (int j = 0; j < n; j++) 
		{
			if (i == j) continue;
			
			int rc = pipe2((int*)&proc_info.pipes[i][j], O_NONBLOCK | O_DIRECT);
			
			if( rc < 0 ) return 1;
			fprintf(pipes_log_f, "opened pipe(%d, %d)\n", i, j);
		}
	}
	
	FILE * events_log_f = fopen(events_log, "a");
	if (events_log_f == NULL) return 1;
	
	// Fork
	pid_t cur_sys_pid = 0;
	local_id cur_pid = 0;
	
	for (local_id pid = 1; pid < n; pid++)
	{
		int sys_pid = fork();
		if( sys_pid < 0 ) return 1;
		if (sys_pid == 0)
		{
			cur_pid = pid;
			cur_sys_pid = getpid();
			break;
		}
	}
	
	/*if (cur_pid)
	{
		parentAction(proc_info);
	}*/
	
	for (int i = 0; i < n; i++)
		free(proc_info.pipes[i]);
	free(proc_info.pipes);
	
	return 0;
}
