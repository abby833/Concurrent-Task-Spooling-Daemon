#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>  
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <fcntl.h>
#include "spooler.h"

int active_jobs = 0;
job_info jobs[MAX_CHILDREN];
int max_jobs = 1;

int main(void){

    mkdir("spool", 0755);
    mkdir("spool/inbox", 0755);
    mkdir("spool/status", 0755);
    mkdir("spool/work", 0755);

    mkfifo("spool/control.fifo", 0666);
    int fifo_fd = open("spool/control.fifo", O_RDWR | O_NONBLOCK);

    while(1){

        char fbuf[256];
        int bytes = read(fifo_fd, fbuf, sizeof(fbuf) - 1);
        if(bytes > 0 ){
            fbuf[bytes] = '\0';
            char *p = fbuf;
            while((p = strstr(p, "scale ")) != NULL){
                int val = atoi(p + 6);
                if (val > 0) {
                    max_jobs = val;
                }
                p += 6;
            }
        }

        save_the_time(time(NULL));

        DIR *dir = opendir("spool/inbox");
        if(dir != NULL){
            struct dirent* entry;

            while((entry = readdir(dir)) != NULL){
                size_t len = strlen(entry->d_name);

                if(len > 8 && strcmp(entry->d_name + len - 8, ".request") == 0){
                    char old_path[512], new_path[512];
                    snprintf(old_path, sizeof(old_path), "spool/inbox/%s", entry->d_name);
                    snprintf(new_path, sizeof(new_path), "spool/work/%s", entry->d_name);
                    if(rename(old_path, new_path) == 0){
                        printf("claimed %s\n", entry->d_name);
                        fflush(stdout);
                    }
                }
            }
            closedir(dir);
        }

        DIR *wdir = opendir("spool/work");
        if(wdir != NULL){
            struct dirent *entry;
            while((entry = readdir(wdir)) != NULL && active_jobs < max_jobs){
                size_t len = strlen(entry->d_name);
                if (len > 8 && strcmp(entry->d_name + len - 8, ".request") == 0) {
                    
                    if (!is_started(entry->d_name)) {
                        char new_path[512];
                        snprintf(new_path, sizeof(new_path), "spool/work/%s", entry->d_name);
                        
                        int p[2];
                        if (pipe(p) == -1) {
                            perror("pipe");
                            continue;
                        }

                        pid_t pid = fork();
                        if(pid == -1){
                            perror("fork");
                            close(p[0]);
                            close(p[1]);
                            continue;
                        }

                        if(pid == 0){
                            close(p[1]);
                            
                            char status_path[512];           
                            snprintf(status_path, sizeof(status_path), "spool/status/%s.%d.status", entry->d_name, getpid());

                            char full_request[4096];
                            int bytes_read, total = 0;
                            while((bytes_read = read(p[0], full_request + total, sizeof(full_request) - total - 1)) > 0){
                                total += bytes_read;
                            }
                            full_request[total] = '\0';
                            close(p[0]);

                            write_status(status_path, entry->d_name, getpid(), getppid(), "ACK", NULL, full_request, NULL);

                            char req_copy[4096];
                            strcpy(req_copy, full_request);

                            char *lines[4096];
                            int total_lines = 0;
                            char *token = strtok(req_copy, "\n\r"); 
                            while(token != NULL && total_lines < 100){
                                if(strlen(token) > 0) { 
                                    lines[total_lines++] = token;
                                }
                                token = strtok(NULL, "\n\r");
                            }

                            char echo_mem[256] = "";
                            for(int i = 0; i < total_lines; ++i){
                                char step_buf[256];
                                snprintf(step_buf, sizeof(step_buf), "%d/%d/ %s", i + 1, total_lines, lines[i]);

                                write_status(status_path, entry->d_name, getpid(), getppid(), "RUNNING", step_buf, full_request, echo_mem[0] ? echo_mem : NULL);

                                char cmd[256] = "", arg[256] = "";
                                sscanf(lines[i], "%s %[^\n]", cmd, arg);
                                if(strcmp(cmd, "sleep") == 0){
                                    sleep(atoi(arg));
                                }else if (strcmp(cmd, "echo") == 0){
                                    strncpy(echo_mem, arg, sizeof(echo_mem) - 1);
                                }else if (strcmp(cmd, "fail") == 0){
                                    write_status(status_path, entry->d_name, getpid(), getppid(), "ERROR : fail command", step_buf, full_request, echo_mem[0] ? echo_mem: NULL);
                                    exit(1);
                                }else if(strcmp(cmd, "timeout") == 0){
                                    
                                }
                                else if(strlen(cmd) > 0){
                                    write_status(status_path, entry->d_name, getpid(), getppid(), "ERROR : unknown command", step_buf, full_request, echo_mem[0] ? echo_mem : NULL);
                                    exit(1);
                                }
                            }
                            write_status(status_path, entry->d_name, getpid(), getppid(), "DONE", NULL, full_request, echo_mem[0] ? echo_mem : NULL);
                            exit(0);
                        }
                        else{
                            close(p[0]);
                            
                            add_child(pid, entry->d_name, time(NULL), extract_timeout(new_path), 0);

                            FILE *request_file = fopen(new_path, "r");
                            if(request_file){
                                char buf[252];
                                size_t bytes_read;
                                while((bytes_read = fread(buf, sizeof(char), sizeof(buf), request_file)) > 0){
                                    write(p[1], buf, bytes_read);
                                }
                                fclose(request_file); 
                            }
                            close(p[1]);
                        }
                    }
                }
            }
            closedir(wdir);
        }

        int wstatus;
        pid_t wpid;
        while((wpid = waitpid(-1, &wstatus, WNOHANG)) > 0){
            for(int i = 0; i < active_jobs; ++i){
                if(jobs[i].pid == wpid){
                    if(WIFSIGNALED(wstatus)){
                        int sig = WTERMSIG(wstatus);
                        char st_path[512];
                        snprintf(st_path, sizeof(st_path), "spool/status/%s.%d.status", jobs[i].req_name, wpid);

                        if(jobs[i].ter_sent > 0 && (sig == SIGTERM || sig == SIGKILL)){
                            write_status(st_path, jobs[i].req_name, wpid, getpid(), "TIMEOUT", NULL, NULL, NULL);
                        }else{
                            char err_msg[256];
                            snprintf(err_msg, sizeof(err_msg), "ERROR signal %d", sig);
                            write_status(st_path, jobs[i].req_name, wpid, getpid(), err_msg, NULL, NULL, NULL);
                        }
                    }
                    jobs[i] = jobs[active_jobs - 1];
                    active_jobs --;
                    break;
                }
            }
        }
        sleep(1);
    } 
    
    return 0;
} 
