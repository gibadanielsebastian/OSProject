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

    // Write the treasure to the file
    if (write(treasureFile, &treasure, sizeof(Treasure)) != sizeof(Treasure))
    {
        perror("Error writing to treasure file.\n");
        close(treasureFile);
        return;
    }
    close(treasureFile);
    printf("Treasure added successfully.\n");

    // Log the addition of the treasure
    char logPath[1024];
    sprintf(logPath, "%s/log.txt", huntID);
    int logFile = open(logPath, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (logFile == -1)
    {
        perror("Error opening log file.\n");
        return;
    }

    // Get the current time and format it
    char logEntry[1024];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(logEntry, sizeof(logEntry), "%Y-%m-%d %H:%M:%S", tm_info);

    // Write the log entry
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

    // Create a symbolic link to the treasure log file

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

    // Create the symbolic link
    makeSymbolicLink(logPath, logPathLink);
}

int isValidHuntID(char *huntID)
{
    // Check if huntID follows the format "HuntXXX" where XXX are numbers
    if (strncmp(huntID, "Hunt", 4) != 0)
    {
        printf("Invalid hunt ID format. Hunt ID should be in format 'HuntXXX' where XXX are numbers.\n");
        return 0;
    }

    // Check if the rest of the string contains only digits
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
    // Auto-generate ID by counting existing treasures
    int id = 1;
    char treasurePath[1024];
    sprintf(treasurePath, "%s/treasures.dat", path);
    int treasureFile = open(treasurePath, O_RDONLY);
    if (treasureFile != -1)
    {
        // File exists, count treasures
        Treasure temp;
        while (read(treasureFile, &temp, sizeof(Treasure)) == sizeof(Treasure))
        {
            id++;
        }
        close(treasureFile);
    }
    printf("Treasure ID: %d (auto-generated)\n", id);

    // Get and validate user name - must be unique
    char userName[20];
    int validUserName = 0;
    while (!validUserName)
    {
        printf("User Name (max 19 chars): ");
        scanf("%19s", userName);
        while (getchar() != '\n')
            ; // Clear input buffer

        if (strlen(userName) == 0)
        {
            printf("Error: User name cannot be empty. Try again.\n");
            continue;
        }

        // Check if username already exists in treasures.dat
        int treasureFile = open(treasurePath, O_RDONLY);
        int userExists = 0;

        if (treasureFile != -1)
        {
            Treasure temp;
            while (read(treasureFile, &temp, sizeof(Treasure)) == sizeof(Treasure))
            {
                if (strcmp(temp.userName, userName) == 0)
                {
                    userExists = 1;
                    break;
                }
            }
            close(treasureFile);
        }

        if (userExists)
        {
            printf("Error: User name '%s' already exists. Please choose a different name.\n", userName);
        }
        else
        {
            validUserName = 1;
        }
    }

    // Get and validate coordinates
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

    // Get and validate clue
    char clue[1024];
    int validClue = 0;
    while (!validClue)
    {
        printf("Clue: ");
        scanf(" %1023[^\n]", clue); // Read until newline
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

    // Get and validate value
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
    // Check if the program is used correctly
    if (argc == 1 || (strcmp(argv[1], "add") != 0 && strcmp(argv[1], "list") != 0 && strcmp(argv[1], "view") != 0 && strcmp(argv[1], "remove") != 0))
    {
        printf("Invalid command. Usage: ./a.exe <add | list | view | remove>\n");
        return 0;
    }

    // Adding a treasure to a Hunt
    if (strcmp(argv[1], "add") == 0 && argc == 2)
    {
        printf("Invalid command. Usage: ./a.exe add <HuntID>\n");
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
            DIR *dir = opendir(path);
            if (dir == NULL)
            {
                mkdir(path, 0644);

                getTreasureInfo(path);
            }
            else
            {
                getTreasureInfo(path);
            }
        }
    }

    // Listing treasures in a Hunt
    if (strcmp(argv[1], "list") == 0 && argc != 3)
    {
        printf("Invalid command. Usage: ./a.exe list <HuntID>\n");
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

            // Check if the hunt directory exists and is a directory
            if (!S_ISDIR(huntStat.st_mode))
            {
                printf("Hunt directory does not exist.\n");
                return 0;
            }

            // Print info about the hunt directory
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

            // Read and print the treasures from the file
            Treasure treasure;
            while (read(treasureFile, &treasure, sizeof(Treasure)) == sizeof(Treasure))
            {
                printf("ID: %d, User: %s, Coordinate: (%.2f, %.2f), Clue: %s, Value: %d\n",
                       treasure.id, treasure.userName, treasure.coord.x, treasure.coord.y,
                       treasure.clue, treasure.value);
            }
            close(treasureFile);

            // Log the listing of treasures
            char logPath[1024];
            sprintf(logPath, "Hunts/%s/log.txt", argv[2]);
            int logFile = open(logPath, O_WRONLY | O_APPEND, 0644);
            if (logFile == -1)
            {
                perror("Error opening log file.\n");
                return 0;
            }

            // Get the current time and format it
            char logEntry[1024];
            time_t now = time(NULL);
            tm_info = localtime(&now);
            strftime(logEntry, sizeof(logEntry), "%Y-%m-%d %H:%M:%S", tm_info);

            // Write the log entry
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

    // Viewing information about a treasure in a Hunt by ID
    if (strcmp(argv[1], "view") == 0 && argc != 4)
    {
        printf("Invalid command. Usage: ./a.exe view <HuntID> <TreasureID>\n");
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

            // Log the viewing of the treasure
            char logPath[1024];
            sprintf(logPath, "Hunts/%s/log.txt", argv[2]);
            int logFile = open(logPath, O_WRONLY | O_APPEND, 0644);
            if (logFile == -1)
            {
                perror("Error opening log file.\n");
                return 0;
            }

            // Get the current time and format it
            char logEntry[1024];
            time_t now = time(NULL);
            struct tm *tm_info = localtime(&now);
            strftime(logEntry, sizeof(logEntry), "%Y-%m-%d %H:%M:%S", tm_info);

            // Write the log entry
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

    // Removing a Hunt or a treasure from a Hunt by ID
    if (strcmp(argv[1], "remove") == 0 && argc != 3 && argc != 4)
    {
        printf("Invalid command. Usage: ./a.exe remove <HuntID> [TreasureID]\n");
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

            // Remove the hunt directory and its contents
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

            // No need of logging here as the hunt directory is removed completely, therefore so is the log file.
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

            // Check if the treasure exists
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

            // Create a temporary file to store remaining treasures
            char tempPath[1024];
            sprintf(tempPath, "Hunts/%s/temp.dat", argv[2]);
            int tempFile = open(tempPath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (tempFile == -1)
            {
                perror("Error creating temporary file.\n");
                close(treasureFile);
                return 0;
            }

            // Reset the file pointer to the beginning
            lseek(treasureFile, 0, SEEK_SET);

            // Copy all treasures except the one to be deleted and update IDs accordingly
            while (read(treasureFile, &treasure, sizeof(Treasure)) == sizeof(Treasure))
            {
                if (treasure.id != treasureID)
                {
                    // Decrement IDs of treasures that come after the deleted one
                    if (treasure.id > treasureID)
                    {
                        treasure.id--;
                    }
                    write(tempFile, &treasure, sizeof(Treasure));
                }
            }

            close(treasureFile);
            close(tempFile);

            // Remove the original treasures file and rename the temporary file
            remove(treasuresPath);
            rename(tempPath, treasuresPath);

            printf("Treasure with ID %d removed successfully from Hunt %s.\n", treasureID, argv[2]);

            // Log the removal of the treasure
            char logPath[1024];
            sprintf(logPath, "Hunts/%s/log.txt", argv[2]);
            int logFile = open(logPath, O_WRONLY | O_APPEND, 0644);
            if (logFile == -1)
            {
                perror("Error opening log file.\n");
                return 0;
            }

            // Get the current time and format it
            char logEntry[1024];
            time_t now = time(NULL);
            struct tm *tm_info = localtime(&now);
            strftime(logEntry, sizeof(logEntry), "%Y-%m-%d %H:%M:%S", tm_info);

            // Write the log entry
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
}
