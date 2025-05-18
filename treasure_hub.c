#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MAX_COMMAND_LEN 2048
#define MAX_RESPONSE_LEN 4096
#define COMMAND_FILE "monitor_command.txt"
#define RESPONSE_FILE "monitor_response.txt"

pid_t monitor_pid = -1;
int monitor_running = 0;
int waiting_for_monitor = 0;

int mon_to_main_pipe[2]; // Monitor to Main process pipe
int main_to_mon_pipe[2]; // Main to Monitor process pipe

void monitor_response_handler(int signum) {
    char buffer[MAX_RESPONSE_LEN];
    ssize_t bytes_read = read(mon_to_main_pipe[0], buffer, sizeof(buffer) - 1);
    
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        printf("%s", buffer);
    }
    
    waiting_for_monitor = 0;
}

void monitor_terminated_handler(int signum) {
    int status;
    pid_t pid;
    
    pid = waitpid(monitor_pid, &status, WNOHANG);
    if (pid == monitor_pid) {
        printf("Monitor process has terminated.\n");
        monitor_running = 0;
        waiting_for_monitor = 0;
        monitor_pid = -1;
        
        close(mon_to_main_pipe[0]);
        close(main_to_mon_pipe[1]);
    }
}

void handle_command(int signum) {
    // Not needed anymore as we use pipes directly
}

void monitor_process() {
    struct sigaction sa;
    
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL);
    
    sa.sa_handler = handle_command;
    sigaction(SIGUSR2, &sa, NULL);
    
    close(mon_to_main_pipe[0]); 
    close(main_to_mon_pipe[1]);
    
    printf("Monitor process started (PID: %d)\n", getpid());
    
    while (1) {
        char command[MAX_COMMAND_LEN];
        memset(command, 0, sizeof(command));
        
        // Read command from the pipe
        ssize_t bytes_read = read(main_to_mon_pipe[0], command, sizeof(command) - 1);
        if (bytes_read <= 0) {
            continue;
        }
        
        command[bytes_read] = '\0';
        
        char response[MAX_RESPONSE_LEN];
        memset(response, 0, sizeof(response));
        
        if (strncmp(command, "list_hunts", 10) == 0) {
            snprintf(response, sizeof(response), "=== Available Hunts ===\n");
            
            char buffer[1024];
            FILE *fp = popen("ls -1 Hunts/ 2>/dev/null", "r");
            if (fp != NULL) {
                int count = 0;
                while (fgets(buffer, sizeof(buffer), fp) != NULL) {
                    buffer[strcspn(buffer, "\n")] = '\0';
                    char treasure_path[2048];
                    snprintf(treasure_path, sizeof(treasure_path), "Hunts/%s/treasures.dat", buffer);
                    
                    struct stat st;
                    if (stat(treasure_path, &st) == 0) {
                        int treasures = st.st_size / sizeof(struct {
                            int id;
                            char userName[20];
                            struct { float x, y; } coord;
                            char clue[1024];
                            int value;
                        });
                        
                        char temp[2048];
                        snprintf(temp, sizeof(temp), "Hunt: %s, Treasures: %d\n", buffer, treasures);
                        strcat(response, temp);
                        count++;
                    }
                }
                pclose(fp);
                
                char temp[256];
                snprintf(temp, sizeof(temp), "\nTotal Hunts: %d\n", count);
                strcat(response, temp);
            } else {
                strcat(response, "No hunts found or error accessing directory.\n");
            }
        } 
        else if (strncmp(command, "list_treasures", 14) == 0) {
            char hunt_id[100];
            if (sscanf(command, "list_treasures %s", hunt_id) == 1) {
                char cmd[2048];
                snprintf(cmd, sizeof(cmd), "./treasure_manager list %s 2>&1", hunt_id);
                
                FILE *fp = popen(cmd, "r");
                if (fp != NULL) {
                    char buffer[1024];
                    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
                        strcat(response, buffer);
                    }
                    pclose(fp);
                } else {
                    strcat(response, "Error executing command.\n");
                }
            } else {
                strcat(response, "Invalid command format. Use: list_treasures <HuntID>\n");
            }
        }
        else if (strncmp(command, "view_treasure", 13) == 0) {
            char hunt_id[100];
            int treasure_id;
            if (sscanf(command, "view_treasure %s %d", hunt_id, &treasure_id) == 2) {
                char cmd[2048];
                snprintf(cmd, sizeof(cmd), "./treasure_manager view %s %d 2>&1", hunt_id, treasure_id);
                
                FILE *fp = popen(cmd, "r");
                if (fp != NULL) {
                    char buffer[1024];
                    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
                        strcat(response, buffer);
                    }
                    pclose(fp);
                } else {
                    strcat(response, "Error executing command.\n");
                }
            } else {
                strcat(response, "Invalid command format. Use: view_treasure <HuntID> <TreasureID>\n");
            }
        }
        else if (strncmp(command, "calculate_score", 15) == 0) {
            char hunt_id[100];
            if (sscanf(command, "calculate_score %s", hunt_id) == 1) {
                int score_pipe[2];
                if (pipe(score_pipe) != 0) {
                    strcat(response, "Error creating pipe for score calculation.\n");
                } else {
                    pid_t score_pid = fork();
                    
                    if (score_pid < 0) {
                        strcat(response, "Error forking process for score calculation.\n");
                        close(score_pipe[0]);
                        close(score_pipe[1]);
                    } else if (score_pid == 0) {
                        close(score_pipe[0]); 
                        
                        dup2(score_pipe[1], STDOUT_FILENO);
                        close(score_pipe[1]);
                        
                        execl("./calculate_score", "./calculate_score", hunt_id, NULL);
                        
                        fprintf(stderr, "Failed to execute score calculator\n");
                        exit(1);
                    } else {
                        close(score_pipe[1]);
                        
                        char buffer[4096];
                        ssize_t bytes_read;
                        
                        while ((bytes_read = read(score_pipe[0], buffer, sizeof(buffer) - 1)) > 0) {
                            buffer[bytes_read] = '\0';
                            strcat(response, buffer);
                        }
                        
                        close(score_pipe[0]);
                        
                        int status;
                        waitpid(score_pid, &status, 0);
                        
                        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
                            strcat(response, "Score calculation failed.\n");
                        }
                    }
                }
            } else {
                strcat(response, "Invalid command format. Use: calculate_score <HuntID>\n");
            }
        }
        else if (strcmp(command, "stop_monitor") == 0) {
            strcpy(response, "Monitor process stopping...\n");
            
            write(mon_to_main_pipe[1], response, strlen(response));
            
            close(mon_to_main_pipe[1]);
            close(main_to_mon_pipe[0]);
            
            kill(getppid(), SIGUSR1);
            sleep(1);
            
            exit(0);
        }
        else {
            snprintf(response, sizeof(response), "Unknown command: %s\n", command);
        }
        
        write(mon_to_main_pipe[1], response, strlen(response));
        
        kill(getppid(), SIGUSR1);
    }
}

void send_command_to_monitor(const char *command) {
    if (!monitor_running) {
        printf("Error: Monitor is not running.\n");
        return;
    }
    
    write(main_to_mon_pipe[1], command, strlen(command));
    
    waiting_for_monitor = 1;
    
    kill(monitor_pid, SIGUSR2);
    
    int timeout = 10;
    while (waiting_for_monitor && timeout > 0) {
        sleep(1);
        timeout--;
    }
    
    if (waiting_for_monitor) {
        printf("Timeout waiting for monitor response.\n");
        waiting_for_monitor = 0;
    }
}

int main() {
    struct sigaction sa;
    sa.sa_handler = monitor_response_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL);
    
    sa.sa_handler = monitor_terminated_handler;
    sigaction(SIGCHLD, &sa, NULL);
    
    printf("Treasure Hunt Hub\n");
    printf("=================\n");
    printf("Available commands:\n");
    printf("  start_monitor - Start the monitor process\n");
    printf("  list_hunts - List all available hunts\n");
    printf("  list_treasures <HuntID> - List treasures in a hunt\n");
    printf("  view_treasure <HuntID> <TreasureID> - View a specific treasure\n");
    printf("  calculate_score <HuntID> - Calculate scores for users in a hunt\n");
    printf("  stop_monitor - Stop the monitor process\n");
    printf("  exit - Exit the treasure hub\n\n");
    
    char command[MAX_COMMAND_LEN];
    
    while (1) {
        printf("> ");
        if (fgets(command, sizeof(command), stdin) == NULL) {
            break;
        }
        
        command[strcspn(command, "\n")] = '\0';
        
        if (strcmp(command, "start_monitor") == 0) {
            if (monitor_running) {
                printf("Monitor is already running.\n");
                continue;
            }
            
            if (pipe(mon_to_main_pipe) != 0 || pipe(main_to_mon_pipe) != 0) {
                perror("Failed to create pipes");
                continue;
            }
            
            pid_t pid = fork();
            
            if (pid < 0) {
                perror("Fork failed");
                
                close(mon_to_main_pipe[0]);
                close(mon_to_main_pipe[1]);
                close(main_to_mon_pipe[0]);
                close(main_to_mon_pipe[1]);
                continue;
            } else if (pid == 0) {
                monitor_process();
                exit(0);
            } else {
                monitor_pid = pid;
                monitor_running = 1;
                
                close(mon_to_main_pipe[1]);
                close(main_to_mon_pipe[0]);
                
                printf("Monitor started with PID: %d\n", pid);
            }
        }
        else if (strncmp(command, "list_hunts", 10) == 0) {
            if (!monitor_running) {
                printf("Error: Monitor is not running. Use 'start_monitor' first.\n");
                continue;
            }
            send_command_to_monitor("list_hunts");
        }
        else if (strncmp(command, "list_treasures", 14) == 0) {
            if (!monitor_running) {
                printf("Error: Monitor is not running. Use 'start_monitor' first.\n");
                continue;
            }
            send_command_to_monitor(command);
        }
        else if (strncmp(command, "view_treasure", 13) == 0) {
            if (!monitor_running) {
                printf("Error: Monitor is not running. Use 'start_monitor' first.\n");
                continue;
            }
            send_command_to_monitor(command);
        }
        else if (strncmp(command, "calculate_score", 15) == 0) {
            if (!monitor_running) {
                printf("Error: Monitor is not running. Use 'start_monitor' first.\n");
                continue;
            }
            send_command_to_monitor(command);
        }
        else if (strcmp(command, "stop_monitor") == 0) {
            if (!monitor_running) {
                printf("Monitor is not running.\n");
                continue;
            }
            send_command_to_monitor("stop_monitor");
        }
        else if (strcmp(command, "exit") == 0) {
            if (monitor_running) {
                printf("Stopping monitor process before exit...\n");
                send_command_to_monitor("stop_monitor");
                sleep(1);
                
                if (monitor_running) {
                    kill(monitor_pid, SIGTERM);
                    sleep(1);
                }
                
                close(mon_to_main_pipe[0]);
                close(main_to_mon_pipe[1]);
            }
            break;
        }
        else {
            printf("Unknown command: %s\n", command);
        }
    }
    
    printf("Exiting Treasure Hub.\n");
    return 0;
}