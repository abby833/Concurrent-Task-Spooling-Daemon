#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>  
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>

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

    fprintf(f, "--- request ---\n%s", full_req);
    fclose(f);
    rename(tmp_path, path);
}
int main(void){

    mkdir("spool", 0755);
    mkdir("spool/inbox", 0755);
    mkdir("spool/status", 0755);
    mkdir("spool/work", 0755);

    while(1){
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

                    int p[2];
                    ///p[0] for reading and p[1] for wrting
                    if (pipe(p) == -1) {
                        perror("pipe");
                        continue;
                    }

                    pid_t pid = fork();
                    if(pid == -1){ //fork failed, memory full
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
                        char *token = strtok(req_copy, "\n");
                        while(token != NULL && total_lines < 100){
                            lines[total_lines++] = token;
                            token = strtok(NULL, "\n");
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
                            }else if(strlen(cmd) > 0){
                                write_status(status_path, entry->d_name, getpid(), getppid(), "ERROR : unknown command", step_buf, full_request, echo_mem[0] ? echo_mem : NULL);
                            }
                        }
                        write_status(status_path, entry->d_name, getpid(), getppid(), "DONE", NULL, full_request, echo_mem[0] ? echo_mem : NULL);
                        exit(0);
                    }
                    else{
                        close(p[0]);
                        FILE *request_file = fopen(new_path, "r");
                        if(request_file){
                            char buf[252];
                            size_t bytes_read;
                            while((bytes_read = fread(buf, sizeof(char), sizeof(buf), request_file)) > 0){
                                write(p[1], buf, bytes_read);
                            }
                        }
                        fclose(request_file);
                        close(p[1]);
                    }
                }
            }
            closedir(dir);
        }
        sleep(1);
        while(waitpid(-1, NULL, WNOHANG) > 0);
    }
    return 0;
}
