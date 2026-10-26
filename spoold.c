#include "spooler.h"

int is_started(const char *req_name){
    for(int i = 0; i < active_jobs; ++i){
        if(strcmp(jobs[i].req_name, req_name) == 0)
            return 1;
    }

    DIR *d = opendir("spool/status");
    if(!d) return 0;
    
    struct dirent *e;
    int status = 0;
    while((e = readdir(d)) != NULL){
        if(strncmp(e->d_name, req_name, strlen(req_name)) == 0){
            status = 1;
            break;
        }
    }
    closedir(d);
    return status;
}

void add_child(int pid, const char *req_name, time_t time, int time_out, time_t term_sent){
    if (active_jobs >= MAX_CHILDREN) return; 
    
    jobs[active_jobs].pid = pid;
    strcpy(jobs[active_jobs].req_name, req_name);
    jobs[active_jobs].start_time = time;
    jobs[active_jobs].timeout_seconds = time_out;
    jobs[active_jobs].ter_sent = term_sent;
    active_jobs ++;
}

void save_the_time(time_t now){
    for(int i = 0; i < active_jobs; ++i){
        if(now - jobs[i].start_time > jobs[i].timeout_seconds){
            if(jobs[i].ter_sent == 0){
                kill(jobs[i].pid, SIGTERM);
                jobs[i].ter_sent = now;
            } else if(now - jobs[i].ter_sent >= 3){
                kill(jobs[i].pid, SIGKILL);
            }
        }
    }
}

int extract_timeout(const char *filepath){
    FILE *f = fopen(filepath, "r");
    if(!f) return DEFAULT_TIMEOUT;

    char linie[256];
    int to = DEFAULT_TIMEOUT;
    while(fgets(linie, sizeof(linie), f)){
        if(strncmp(linie, "timeout ", 8) == 0){
            to = atoi(linie + 8);
            break;
        }
    }
    fclose(f);
    return to;
}

void write_status(const char* path, const char*req_name, int pid, int ppid, const char* status, const char* step_info, const char* full_req, const char* echo_text){
    char tmp_path[512];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

    FILE *f = fopen(tmp_path, "w");
    if(!f) return ;

    fprintf(f, "pid: %d\n", pid);
    fprintf(f, "ppid: %d\n", ppid);
    fprintf(f, "request: %s\n", req_name);
    fprintf(f, "status: %s\n", status);

    if (step_info) fprintf(f, "step: %s\n", step_info);
    if (echo_text) fprintf(f, "output: %s\n", echo_text);

    if(full_req)
        fprintf(f, "--- request ---\n%s", full_req);
    else
        fprintf(f, "--- request ---\nThe data is nod available :( \n");
    fclose(f);
    rename(tmp_path, path);
}
