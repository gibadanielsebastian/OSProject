#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

typedef struct {
    int id;
    char userName[20];
    struct { float x, y; } coord;
    char clue[1024];
    int value;
} Treasure;

typedef struct {
    char userName[20];
    int totalValue;
    int treasureCount;
} UserScore;

void addOrUpdateUserScore(UserScore **scores, int *scoreCount, char *userName, int value) {
    for (int i = 0; i < *scoreCount; i++) {
        if (strcmp((*scores)[i].userName, userName) == 0) {
            (*scores)[i].totalValue += value;
            (*scores)[i].treasureCount++;
            return;
        }
    }
    
    *scores = realloc(*scores, (*scoreCount + 1) * sizeof(UserScore));
    strcpy((*scores)[*scoreCount].userName, userName);
    (*scores)[*scoreCount].totalValue = value;
    (*scores)[*scoreCount].treasureCount = 1;
    (*scoreCount)++;
}

void logScoreCalculation(const char *huntID) {
    char logPath[1024];
    sprintf(logPath, "Hunts/%s/log.txt", huntID);
    
    int logFile = open(logPath, O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (logFile == -1) {
        perror("Error opening log file");
        return;
    }
    
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timeStr[100];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", tm_info);
    
    char logMsg[2048];
    sprintf(logMsg, "%s - Calculated scores for hunt %s.\n", timeStr, huntID);
    
    write(logFile, logMsg, strlen(logMsg));
    close(logFile);
}

int compareScores(const void *a, const void *b) {
    UserScore *userA = (UserScore *)a;
    UserScore *userB = (UserScore *)b;
    
    return userB->totalValue - userA->totalValue;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <HuntID>\n", argv[0]);
        return 1;
    }
    
    char *huntID = argv[1];
    
    if (strncmp(huntID, "Hunt", 4) != 0) {
        printf("Invalid hunt ID format. Hunt ID should start with 'Hunt'.\n");
        return 1;
    }
    
    char huntPath[1024];
    sprintf(huntPath, "Hunts/%s", huntID);
    
    if (access(huntPath, F_OK) != 0) {
        printf("Hunt %s does not exist.\n", huntID);
        return 1;
    }
    
    char treasurePath[1024];
    sprintf(treasurePath, "Hunts/%s/treasures.dat", huntID);
    
    int treasureFile = open(treasurePath, O_RDONLY);
    if (treasureFile == -1) {
        printf("Error: Could not open treasures file for hunt %s.\n", huntID);
        return 1;
    }
    
    Treasure treasure;
    UserScore *scores = NULL;
    int scoreCount = 0;
    
    while (read(treasureFile, &treasure, sizeof(Treasure)) == sizeof(Treasure)) {
        addOrUpdateUserScore(&scores, &scoreCount, treasure.userName, treasure.value);
    }
    
    close(treasureFile);
    
    qsort(scores, scoreCount, sizeof(UserScore), compareScores);
    
    printf("=== Score Report for Hunt %s ===\n\n", huntID);
    
    if (scoreCount == 0) {
        printf("No treasures found in this hunt.\n");
    } else {
        printf("User Rankings:\n");
        printf("%-20s %-15s %-15s\n", "Username", "Total Value", "# of Treasures");
        printf("------------------------------------------------\n");
        
        for (int i = 0; i < scoreCount; i++) {
            printf("%-20s %-15d %-15d\n", 
                   scores[i].userName, 
                   scores[i].totalValue, 
                   scores[i].treasureCount);
        }
        
        printf("\nTotal Users: %d\n", scoreCount);
    }
    
    logScoreCalculation(huntID);
    
    free(scores);
    
    return 0;
}