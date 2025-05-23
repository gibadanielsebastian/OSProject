#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

typedef struct
{
    float x, y;
} Coordinate;

typedef struct
{
    int id;
    char userName[20];
    Coordinate coord;
    char clue[1024];
    int value;
} Treasure;

int hasWritePermission(const char *path)
{
    if (access(path, W_OK) == 0) {
        return 1; // Has write permission
    }
    return 0; // No write permission
}

int ensureHuntsDirectory()
{
    DIR *dir = opendir("Hunts");
    if (dir == NULL) {
        if (mkdir("Hunts", 0755) != 0) {
            perror("Error creating Hunts directory");
            return 0;
        }
        printf("Created Hunts directory\n");
        return 1;
    }
    
    closedir(dir);
    
    if (!hasWritePermission("Hunts")) {
        printf("No write permission for Hunts directory. Recreating...\n");
        
        char oldPath[256];
        sprintf(oldPath, "Hunts_old_%ld", (long)time(NULL));
        if (rename("Hunts", oldPath) != 0) {
            perror("Error renaming existing Hunts directory");
            return 0;
        }
        
        if (mkdir("Hunts", 0755) != 0) {
            perror("Error creating new Hunts directory");
            return 0;
        }
        
        printf("Recreated Hunts directory with proper permissions\n");
    }
    
    return 1;
}

int ensureHuntDirectory(const char *huntID)
{
    char path[1024];
    sprintf(path, "Hunts/%s", huntID);
    
    DIR *dir = opendir(path);
    if (dir == NULL) {
        if (mkdir(path, 0755) != 0) {
            perror("Error creating hunt directory");
            return 0;
        }
        printf("Created hunt directory: %s\n", path);
        return 1;
    }
    
    closedir(dir);
    
    if (!hasWritePermission(path)) {
        printf("No write permission for hunt directory. Recreating...\n");
        
        char oldPath[1280];
        sprintf(oldPath, "%s_old_%ld", path, (long)time(NULL));
        if (rename(path, oldPath) != 0) {
            perror("Error renaming existing hunt directory");
            return 0;
        }
        
        if (mkdir(path, 0755) != 0) {
            perror("Error creating new hunt directory");
            return 0;
        }
        DIR *oldDir = opendir(oldPath);
        if (oldDir != NULL) {
            struct dirent *entry;
            while ((entry = readdir(oldDir)) != NULL) {
                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                    continue;
                
                char oldFilePath[2048], newFilePath[2048];
                sprintf(oldFilePath, "%s/%s", oldPath, entry->d_name);
                sprintf(newFilePath, "%s/%s", path, entry->d_name);
                
                FILE *oldFile = fopen(oldFilePath, "rb");
                if (oldFile != NULL) {
                    FILE *newFile = fopen(newFilePath, "wb");
                    if (newFile != NULL) {
                        char buffer[4096];
                        size_t bytes;
                        while ((bytes = fread(buffer, 1, sizeof(buffer), oldFile)) > 0) {
                            fwrite(buffer, 1, bytes, newFile);
                        }
                        fclose(newFile);
                    }
                    fclose(oldFile);
                }
            }
            closedir(oldDir);
        }
        
        printf("Recreated hunt directory with proper permissions: %s\n", path);
    }
    
    return 1;
}

void makeSymbolicLink(char *path, char *linkPath)
{
    FILE *file = fopen(linkPath, "r");
    if (file != NULL)
    {
        fclose(file);
        remove(linkPath);
    }

    if (symlink(path, linkPath) == -1)
    {
        perror("Error creating symbolic link");
    }
}

void addTreasure(char *huntID, int id, char *userName, Coordinate coord, char *clue, int value)
{
    char path[1024];
    sprintf(path, "%s/treasures.dat", huntID);
    int treasureFile = open(path, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (treasureFile == -1)
    {
        perror("Error opening treasure file.\n");
        return;
    }

    Treasure treasure;
    treasure.id = id;
    strcpy(treasure.userName, userName);
    treasure.coord = coord;
    strcpy(treasure.clue, clue);
    treasure.value = value;

    if (write(treasureFile, &treasure, sizeof(Treasure)) != sizeof(Treasure))
    {
        perror("Error writing to treasure file.\n");
        close(treasureFile);
        return;
    }
    close(treasureFile);
    printf("Treasure added successfully.\n");

    char logPath[1024];
    sprintf(logPath, "%s/log.txt", huntID);
    int logFile = open(logPath, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (logFile == -1)
    {
        perror("Error opening log file.\n");
        return;
    }

    char logEntry[1024];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(logEntry, sizeof(logEntry), "%Y-%m-%d %H:%M:%S", tm_info);

    char logMessage[2048];
    sprintf(logMessage, "%s - Added Treasure ID: %d, User: %s, Coordinate: (%.2f, %.2f), Clue: %s, Value: %d\n",
            logEntry, id, userName, coord.x, coord.y, clue, value);
    if (write(logFile, logMessage, strlen(logMessage)) != strlen(logMessage))
    {
        perror("Error writing to log file.\n");
        close(logFile);
        return;
    }

    close(logFile);

    char huntName[1024];
    // Extract just the hunt name from the path
    char *huntNamePtr = strrchr(huntID, '/');
    if (huntNamePtr != NULL)
    {
        // If there's a slash, take what comes after it
        strcpy(huntName, huntNamePtr + 1);
    }
    else
    {
        // If there's no slash, use the entire huntID
        strcpy(huntName, huntID);
    }
    char logPathLink[2048];
    sprintf(logPathLink, "log_%s.txt", huntName);

    makeSymbolicLink(logPath, logPathLink);
}

int isValidHuntID(char *huntID)
{
    if (strncmp(huntID, "Hunt", 4) != 0)
    {
        printf("Invalid hunt ID format. Hunt ID should be in format 'HuntXXX' where XXX are numbers.\n");
        return 0;
    }

    int valid = 1;
    for (int i = 4; huntID[i] != '\0'; i++)
    {
        if (huntID[i] < '0' || huntID[i] > '9')
        {
            valid = 0;
            break;
        }
    }

    if (!valid)
    {
        printf("Invalid hunt ID format. Hunt ID should be in format 'HuntXXX' where XXX are numbers.\n");
        return 0;
    }
    return 1;
}

void getTreasureInfo(char *path)
{
    int id = 1;
    char treasurePath[1024];
    sprintf(treasurePath, "%s/treasures.dat", path);
    int treasureFile = open(treasurePath, O_RDONLY);
    if (treasureFile != -1)
    {
        Treasure temp;
        while (read(treasureFile, &temp, sizeof(Treasure)) == sizeof(Treasure))
        {
            id++;
        }
        close(treasureFile);
    }
    printf("Treasure ID: %d (auto-generated)\n", id);

    char userName[20];
    int validUserName = 0;
    while (!validUserName)
    {
        printf("User Name (max 19 chars): ");
        scanf("%19s", userName);
        while (getchar() != '\n')
            ;

        if (strlen(userName) == 0)
        {
            printf("Error: User name cannot be empty. Try again.\n");
            continue;
        }

        validUserName = 1;
    }

    Coordinate coord;
    int validCoord = 0;
    while (!validCoord)
    {
        printf("Coordinate (x y): ");
        if (scanf("%f %f", &coord.x, &coord.y) != 2)
        {
            printf("Error: Invalid coordinates. Please enter two numbers.\n");
            while (getchar() != '\n')
                ;
        }
        else
        {
            validCoord = 1;
            while (getchar() != '\n')
                ;
        }
    }

    char clue[1024];
    int validClue = 0;
    while (!validClue)
    {
        printf("Clue: ");
        scanf(" %1023[^\n]", clue);
        while (getchar() != '\n')
            ;

        if (strlen(clue) == 0)
        {
            printf("Error: Clue cannot be empty. Try again.\n");
        }
        else
        {
            validClue = 1;
        }
    }

    int value;
    int validValue = 0;
    while (!validValue)
    {
        printf("Value (positive number): ");
        if (scanf("%d", &value) != 1 || value <= 0)
        {
            printf("Error: Invalid value. Please enter a positive number.\n");
            while (getchar() != '\n')
                ;
        }
        else
        {
            validValue = 1;
            while (getchar() != '\n')
                ;
        }
    }

    addTreasure(path, id, userName, coord, clue, value);
}

int main(int argc, char *argv[])
{
    if (argc == 1 || (strcmp(argv[1], "add") != 0 && strcmp(argv[1], "list") != 0 && strcmp(argv[1], "view") != 0 && strcmp(argv[1], "remove") != 0))
    {
        printf("Invalid command. Usage: ./treasure_manager <add | list | view | remove>\n");
        return 0;
    }

    if (!ensureHuntsDirectory()) {
        printf("Failed to ensure Hunts directory is accessible. Exiting.\n");
        return 1;
    }

    if (strcmp(argv[1], "add") == 0 && argc == 2)
    {
        printf("Invalid command. Usage: ./treasure_manager add <HuntID>\n");
        return 0;
    }
    else if (strcmp(argv[1], "add") == 0 && argc == 3)
    {
        if (!isValidHuntID(argv[2]))
        {
            return 0;
        }
        else
        {
            char path[1024];
            sprintf(path, "Hunts/%s", argv[2]);
            
            if (!ensureHuntDirectory(argv[2])) {
                printf("Failed to ensure hunt directory is accessible. Exiting.\n");
                return 1;
            }
            
            getTreasureInfo(path);
        }
    }

    if (strcmp(argv[1], "list") == 0 && argc != 3)
    {
        printf("Invalid command. Usage: ./treasure_manager list <HuntID>\n");
        return 0;
    }
    else if (strcmp(argv[1], "list") == 0 && argc == 3)
    {
        if (!isValidHuntID(argv[2]))
        {
            return 0;
        }
        else
        {
            if (!ensureHuntDirectory(argv[2])) {
                printf("Failed to ensure hunt directory is accessible. Exiting.\n");
                return 1;
            }
            
            char treasuresPath[1024];
            sprintf(treasuresPath, "Hunts/%s/treasures.dat", argv[2]);
            int treasureFile = open(treasuresPath, O_RDONLY);
            if (treasureFile == -1)
            {
                perror("Error opening treasure file.\n");
                return 0;
            }

            char huntPath[1024];
            sprintf(huntPath, "Hunts/%s", argv[2]);
            struct stat huntStat;
            if (stat(huntPath, &huntStat) != 0)
            {
                perror("Error getting hunt directory info.\n");
                return 0;
            }

            if (!S_ISDIR(huntStat.st_mode))
            {
                printf("Hunt directory does not exist.\n");
                return 0;
            }

            printf("Hunt: %s\n", argv[2]);
            // Get the size of the treasures file
            struct stat treasureStat;
            if (fstat(treasureFile, &treasureStat) == 0)
            {
                printf("Total treasure file size: %ld bytes\n", treasureStat.st_size);
            }
            else
            {
                printf("Total treasure file size: unknown\n");
            }
            char timeStr[100];
            struct tm *tm_info = localtime(&huntStat.st_mtime);
            strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", tm_info);
            printf("Last modified: %s\n", timeStr);

            printf("\nTreasures:\n");
            printf("ID\tUser\tCoordinate (x, y)\tClue\tValue\n");
            printf("--------------------------------------------------------\n");

            Treasure treasure;
            while (read(treasureFile, &treasure, sizeof(Treasure)) == sizeof(Treasure))
            {
                printf("ID: %d, User: %s, Coordinate: (%.2f, %.2f), Clue: %s, Value: %d\n",
                       treasure.id, treasure.userName, treasure.coord.x, treasure.coord.y,
                       treasure.clue, treasure.value);
            }
            close(treasureFile);

            char logPath[1024];
            sprintf(logPath, "Hunts/%s/log.txt", argv[2]);
            int logFile = open(logPath, O_WRONLY | O_APPEND | O_CREAT, 0644);
            if (logFile == -1)
            {
                perror("Error opening log file.\n");
                return 0;
            }

            char logEntry[1024];
            time_t now = time(NULL);
            tm_info = localtime(&now);
            strftime(logEntry, sizeof(logEntry), "%Y-%m-%d %H:%M:%S", tm_info);

            char logMessage[2048];
            sprintf(logMessage, "%s - Listed treasures.\n", logEntry);
            if (write(logFile, logMessage, strlen(logMessage)) != strlen(logMessage))
            {
                perror("Error writing to log file.\n");
                close(logFile);
                return 0;
            }

            close(logFile);
        }
    }

    if (strcmp(argv[1], "view") == 0 && argc != 4)
    {
        printf("Invalid command. Usage: ./treasure_manager view <HuntID> <TreasureID>\n");
        return 0;
    }
    else if (strcmp(argv[1], "view") == 0 && argc == 4)
    {
        if (!isValidHuntID(argv[2]))
        {
            return 0;
        }
        else
        {
            char huntPath[1024];
            sprintf(huntPath, "Hunts/%s", argv[2]);
            DIR *dir = opendir(huntPath);
            if (dir == NULL) {
                printf("Hunt directory does not exist or cannot be accessed.\n");
                return 1;
            }
            closedir(dir);
            
            char treasuresPath[1024];
            sprintf(treasuresPath, "Hunts/%s/treasures.dat", argv[2]);
            int treasureFile = open(treasuresPath, O_RDONLY);
            if (treasureFile == -1)
            {
                perror("Error opening treasure file.\n");
                return 0;
            }

            int treasureID = atoi(argv[3]);
            Treasure treasure;
            int found = 0;
            while (read(treasureFile, &treasure, sizeof(Treasure)) == sizeof(Treasure))
            {
                if (treasure.id == treasureID)
                {
                    found = 1;
                    printf("ID: %d, User: %s, Coordinate: (%.2f, %.2f), Clue: %s, Value: %d\n",
                           treasure.id, treasure.userName, treasure.coord.x, treasure.coord.y,
                           treasure.clue, treasure.value);
                    break;
                }
            }
            if (!found)
            {
                printf("Treasure with ID %d not found in Hunt %s.\n", treasureID, argv[2]);
            }
            close(treasureFile);

            if (!ensureHuntDirectory(argv[2])) {
                printf("Warning: Cannot log this view operation due to permission issues.\n");
                return 0;
            }

            char logPath[1024];
            sprintf(logPath, "Hunts/%s/log.txt", argv[2]);
            int logFile = open(logPath, O_WRONLY | O_APPEND | O_CREAT, 0644);
            if (logFile == -1)
            {
                perror("Error opening log file.\n");
                return 0;
            }

            char logEntry[1024];
            time_t now = time(NULL);
            struct tm *tm_info = localtime(&now);
            strftime(logEntry, sizeof(logEntry), "%Y-%m-%d %H:%M:%S", tm_info);

            char logMessage[2048];
            sprintf(logMessage, "%s - Viewed Treasure ID: %d.\n", logEntry, treasureID);
            if (write(logFile, logMessage, strlen(logMessage)) != strlen(logMessage))
            {
                perror("Error writing to log file.\n");
                close(logFile);
                return 0;
            }

            close(logFile);
        }
    }

    if (strcmp(argv[1], "remove") == 0 && argc != 3 && argc != 4)
    {
        printf("Invalid command. Usage: ./treasure_manager remove <HuntID> [TreasureID]\n");
        return 0;
    }
    else if (strcmp(argv[1], "remove") == 0 && argc == 3)
    {
        if (!isValidHuntID(argv[2]))
        {
            return 0;
        }
        else
        {
            char path[1024];
            sprintf(path, "Hunts/%s", argv[2]);
            DIR *dir = opendir(path);
            if (dir == NULL)
            {
                printf("Hunt %s does not exist.\n", argv[2]);
                return 0;
            }
            closedir(dir);

            if (!hasWritePermission(path)) {
                printf("No permission to remove hunt directory. Attempting to force removal...\n");
                chmod(path, 0777);
            }

            char command[2048];
            sprintf(command, "rm -rf \"%s\"", path);
            system(command);

            DIR *dir2 = opendir(path);
            if (dir2 == NULL)
            {
                printf("Hunt %s removed successfully.\n", argv[2]);
                return 0;
            }
            closedir(dir2);
            printf("Failed to remove hunt directory. Check permissions.\n");
        }
    }
    else if (strcmp(argv[1], "remove") == 0 && argc == 4)
    {
        if (!isValidHuntID(argv[2]))
        {
            return 0;
        }
        else
        {
            if (!ensureHuntDirectory(argv[2])) {
                printf("Cannot remove treasure - no write permission for hunt directory.\n");
                return 1;
            }
            
            char treasuresPath[1024];
            sprintf(treasuresPath, "Hunts/%s/treasures.dat", argv[2]);
            int treasureFile = open(treasuresPath, O_RDONLY);
            if (treasureFile == -1)
            {
                perror("Error opening treasure file.\n");
                return 0;
            }

            int treasureID = atoi(argv[3]);
            Treasure treasure;
            int found = 0;

            while (read(treasureFile, &treasure, sizeof(Treasure)) == sizeof(Treasure))
            {
                if (treasure.id == treasureID)
                {
                    found = 1;
                    break;
                }
            }

            if (!found)
            {
                printf("Treasure with ID %d not found in Hunt %s.\n", treasureID, argv[2]);
                close(treasureFile);
                return 0;
            }

            char tempPath[1024];
            sprintf(tempPath, "Hunts/%s/temp.dat", argv[2]);
            int tempFile = open(tempPath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (tempFile == -1)
            {
                perror("Error creating temporary file.\n");
                close(treasureFile);
                return 0;
            }

            lseek(treasureFile, 0, SEEK_SET);

            while (read(treasureFile, &treasure, sizeof(Treasure)) == sizeof(Treasure))
            {
                if (treasure.id != treasureID)
                {
                    if (treasure.id > treasureID)
                    {
                        treasure.id--;
                    }
                    write(tempFile, &treasure, sizeof(Treasure));
                }
            }

            close(treasureFile);
            close(tempFile);

            remove(treasuresPath);
            rename(tempPath, treasuresPath);

            printf("Treasure with ID %d removed successfully from Hunt %s.\n", treasureID, argv[2]);

            char logPath[1024];
            sprintf(logPath, "Hunts/%s/log.txt", argv[2]);
            int logFile = open(logPath, O_WRONLY | O_APPEND | O_CREAT, 0644);
            if (logFile == -1)
            {
                perror("Error opening log file.\n");
                return 0;
            }

            char logEntry[1024];
            time_t now = time(NULL);
            struct tm *tm_info = localtime(&now);
            strftime(logEntry, sizeof(logEntry), "%Y-%m-%d %H:%M:%S", tm_info);

            char logMessage[2048];
            sprintf(logMessage, "%s - Removed Treasure ID: %d from Hunt %s.\n", logEntry, treasureID, argv[2]);
            if (write(logFile, logMessage, strlen(logMessage)) != strlen(logMessage))
            {
                perror("Error writing to log file.\n");
                close(logFile);
                return 0;
            }

            close(logFile);
        }
    }
    
    return 0;
}