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

#define MAX_COMMAND_LEN 1024
#define COMMAND_FILE "monitor_command.txt"
#define RESPONSE_FILE "monitor_response.txt"

pid_t monitor_pid = -1;
int monitor_running = 0;
int waiting_for_monitor = 0;

void monitor_response_handler(int signum) {
    FILE *response_file = fopen(RESPONSE_FILE, "r");
    if (response_file != NULL) {
        char buffer[4096];
        size_t bytes_read;
        
        while ((bytes_read = fread(buffer, 1, sizeof(buffer) - 1, response_file)) > 0) {
            buffer[bytes_read] = '\0';
            printf("%s", buffer);
        }
        fclose(response_file);
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
    }
}

void handle_command(int signum) {
}

void monitor_process() {
    struct sigaction sa;
    
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL);
    
    sa.sa_handler = handle_command;
    sigaction(SIGUSR2, &sa, NULL);
    
    printf("Monitor process started (PID: %d)\n", getpid());
    
    while (1) {
        pause();
        
        FILE *cmd_file = fopen(COMMAND_FILE, "r");
        if (cmd_file == NULL) {
            continue;
        }
        
        char command[MAX_COMMAND_LEN];
        if (fgets(command, sizeof(command), cmd_file) != NULL) {
            command[strcspn(command, "\n")] = '\0';
            
            FILE *response_file = fopen(RESPONSE_FILE, "w");
            if (response_file == NULL) {
                fclose(cmd_file);
                continue;
            }
            
            if (strncmp(command, "list_hunts", 10) == 0) {
                fprintf(response_file, "=== Available Hunts ===\n");
                
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
                            
                            fprintf(response_file, "Hunt: %s, Treasures: %d\n", buffer, treasures);
                            count++;
                        }
                    }
                    pclose(fp);
                    
                    fprintf(response_file, "\nTotal Hunts: %d\n", count);
                } else {
                    fprintf(response_file, "No hunts found or error accessing directory.\n");
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
                            fprintf(response_file, "%s", buffer);
                        }
                        pclose(fp);
                    } else {
                        fprintf(response_file, "Error executing command.\n");
                    }
                } else {
                    fprintf(response_file, "Invalid command format. Use: list_treasures <HuntID>\n");
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
                            fprintf(response_file, "%s", buffer);
                        }
                        pclose(fp);
                    } else {
                        fprintf(response_file, "Error executing command.\n");
                    }
                } else {
                    fprintf(response_file, "Invalid command format. Use: view_treasure <HuntID> <TreasureID>\n");
                }
            }
            else if (strcmp(command, "stop_monitor") == 0) {
                fprintf(response_file, "Monitor process stopping...\n");
                fclose(response_file);
                fclose(cmd_file);
                
                kill(getppid(), SIGUSR1);
                sleep(5);
                exit(0);
            }
            else {
                fprintf(response_file, "Unknown command: %s\n", command);
            }
            
            fclose(response_file);
        }
        fclose(cmd_file);
        
        kill(getppid(), SIGUSR1);
    }
}

void send_command_to_monitor(const char *command) {
    if (!monitor_running) {
        printf("Error: Monitor is not running.\n");
        return;
    }
    
    FILE *cmd_file = fopen(COMMAND_FILE, "w");
    if (cmd_file == NULL) {
        perror("Error opening command file");
        return;
    }
    
    fprintf(cmd_file, "%s\n", command);
    fclose(cmd_file);
    
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
            
            pid_t pid = fork();
            
            if (pid < 0) {
                perror("Fork failed");
                continue;
            } else if (pid == 0) {
                monitor_process();
                exit(0);
            } else {
                monitor_pid = pid;
                monitor_running = 1;
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