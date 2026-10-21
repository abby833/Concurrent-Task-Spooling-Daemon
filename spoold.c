#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>  
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>


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

                        FILE *status_file = fopen(status_path, "w");
                        if(status_file){
                            fprintf(status_file, "pid: %d\n", getpid());

                            fprintf(status_file, "ppid: %d\n", getppid());
                            fprintf(status_file, "request: %s\n", entry->d_name);
                            fprintf(status_file, "status: ACK\n");
                            fprintf(status_file, "--- request ---\n");

                            char buf[256];
                            int bytes_read;
                            while ((bytes_read = read(p[0], buf, sizeof(buf))) > 0) {
                                fwrite(buf, 1, bytes_read, status_file);
                            }
                            fclose(status_file);
                        }
                        close(p[0]);
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
