#ifndef spooler_h
#define spooler_h

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_CHILDREN 64
#define DEFAULT_TIMEOUT 10

typedef struct {
    pid_t pid;
    char req_name[256];
    time_t start_time;
    int timeout_seconds;
    time_t ter_sent;
} job_info;
extern int active_jobs;
extern job_info jobs[MAX_CHILDREN];

void add_child(int pid, const char *req_name, time_t time, int time_out, time_t term_sent);
void save_the_time(time_t now);
int extract_timeout(const char *filepath);
void write_status(const char* path, const char*req_name, int pid, int ppid, const char* status, const char* step_info, const char* full_req, const char* echo_text);

#endif