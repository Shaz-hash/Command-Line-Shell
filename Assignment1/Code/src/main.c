//                           بِسْمِ ٱللَّٰهِ ٱلرَّحْمَٰنِ ٱلرَّحِيمِ

/**
 * @file main.c
 * @brief This is the main file for the shell. It contains the main function and the loop that runs the shell.
 * @version 0.1
 * @date 2023-06-02
 *
 * @copyright Copyright (c) 2023
 *
 */

#include "utils.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <unistd.h>
#include <sys/wait.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdbool.h>
#include <glob.h>
#include <errno.h> // for errno

/*
        NOTES TO DO :

        1) Dont free memory in the checkCmd func
        2) Restructure the function cuz history can execute pipelines & chaining & io , like this (wait maybe you can do this executechaining function ?) Do this in the end

*/

/**
 * @brief This is the main function for the shell. It contains the main loop that runs the shell.
 *
 * @return int
 */

#define MAX_TOKEN_LENGTH 500

// GLOBAL VARIABLES :

bool isFork = false;
int childStatus = 0;
int mainFD = 0;

struct token
{
    char *tokenString;
    int *tokenSize;
};

struct KeyValuePair
{
    char *key;
    char *value;
};

struct StringMap
{
    struct KeyValuePair *data;
    size_t size;
};

// DANGEROUS METHOD THAT CAN ANHILIATE THE TERMINAL FOR THE BUILT IN PROCESS ::
void killMainProcess()
{
    if (isFork)
    {
        exit(1);
    }
    else
    {
        childStatus = 1;
    }
}

// FOLLOWING FUNCTIONS ARE UTILITY FUNCTIONS :

char **getTokens(char *inputStr)
{
    char **tokens = NULL;
    size_t tokenCount = 0;
    char token[MAX_TOKEN_LENGTH] = "";
    size_t tokenIndex = 0;
    int inSingleQuotes = 0;
    bool insideQuotes = false;

    for (size_t i = 0; inputStr[i] != '\0'; i++)
    {

        if (inputStr[i] == '"')
        {
            insideQuotes = !insideQuotes; // Toggle the insideQuotes flag when a double quote is encountered
        }

        if (inputStr[i] == ' ' && !inSingleQuotes && !insideQuotes)
        {
            if (tokenIndex > 0)
            {
                token[tokenIndex] = '\0';
                tokenIndex = 0;
                tokenCount++;
                tokens = realloc(tokens, tokenCount * sizeof(char *));
                tokens[tokenCount - 1] = strdup(token);
                // printf("the new token added : %s \n", tokens[tokenCount - 1]);
            }
        }
        else if (inputStr[i] == '\'')
        {
            inSingleQuotes = !inSingleQuotes;
        }
        else
        {
            token[tokenIndex++] = inputStr[i];
        }
    }

    if (tokenIndex > 0)
    {
        token[tokenIndex] = '\0';
        tokenCount++;
        tokens = realloc(tokens, tokenCount * sizeof(char *));
        tokens[tokenCount - 1] = strdup(token);
    }

    tokens = realloc(tokens, (tokenCount + 1) * sizeof(char *));
    tokens[tokenCount] = NULL; // Null-terminate the array

    return tokens;
}

void freeTokens(char **tokens)
{
    for (size_t i = 0; tokens[i] != NULL; i++)
    {
        free(tokens[i]);
    }
    free(tokens);
}

void closeAllFileDescriptors(int index)
{
    int max_fd = getdtablesize(); // Get the maximum file descriptor value

    for (int fd = index; fd < max_fd; fd++)
    { // Start from fd 3 to avoid standard descriptors
        close(fd);
    }
}

void util_fd()
{
    int max_fd = getdtablesize(); // Get the maximum file descriptor value
    for (int fd = 0; fd < max_fd; fd++)
    {
        int flags = fcntl(fd, F_GETFD);
        if (flags != -1 || errno != EBADF)
        {
            printf("File descriptor %d is open\n", fd);
        }
    }
}
// function to see if the mode is script or interactive (0 ==> interactive ; 1 ==> script) :
char getMode(char **tokenArr)
{
    if (tokenArr[0] != NULL && (strcmp(tokenArr[0], "./shell") == 0))
    {
        return 'S';
    }
    else
    {
        return 'I';
    }
}

int getSize(char **tokenArr)
{
    int i = 0;
    while (tokenArr[i] != NULL)
    {
        i++;
    }
    return i;
}

char **trimArray(char **tokenArr, int startIndex, int endIndex)
{
    char **trimArr = (char **)malloc((endIndex - startIndex + 1) * sizeof(char *));
    int counter = startIndex;
    while (counter <= endIndex)
    {
        trimArr[counter - startIndex] = tokenArr[counter];
        counter++;
    }
    return trimArr;
}

char **trimArrayNull(char **tokenArr, int startIndex, int endIndex)
{
    char **trimArr = (char **)malloc((endIndex - startIndex + 2) * sizeof(char *)); // +2 for the new content and NULL terminator
    if (trimArr == NULL)
    {
        perror("failed to allocate space in trimArrayNull");
        exit(EXIT_FAILURE);
    }

    int counter = startIndex;
    int trimCounter = 0;
    while (counter <= endIndex)
    {
        trimArr[trimCounter] = tokenArr[counter];
        counter++;
        trimCounter++;
    }

    // Add a NULL pointer at the end to terminate the array
    trimArr[trimCounter] = NULL;

    return trimArr;
}

char **mergeStringArrays(char **arr1, char **arr2)
{
    int totalStrings = 0;

    // Calculate the total number of strings in arr1
    while (arr1[totalStrings] != NULL)
    {
        totalStrings++;
    }

    // Calculate the total number of strings in arr2
    int i = 0;
    while (arr2[i] != NULL)
    {
        i++;
    }
    totalStrings += i;

    // Allocate memory for the merged array
    char **mergedArray = (char **)malloc((totalStrings + 1) * sizeof(char *));

    if (mergedArray == NULL)
    {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }

    // Copy strings from arr1 and arr2 to mergedArray
    int j;
    for (j = 0; arr1[j] != NULL; j++)
    {
        mergedArray[j] = strdup(arr1[j]);
        if (mergedArray[j] == NULL)
        {
            perror("Memory allocation failed");
            exit(EXIT_FAILURE);
        }
    }

    for (int k = 0; arr2[k] != NULL; k++, j++)
    {
        mergedArray[j] = strdup(arr2[k]);
        if (mergedArray[j] == NULL)
        {
            perror("Memory allocation failed");
            exit(EXIT_FAILURE);
        }
    }

    // Null-terminate the merged array
    mergedArray[j] = NULL;

    return mergedArray;
}

void removeNullTerminators(char *str)
{
    int len = strlen(str);

    // Iterate through the string and set null terminators to spaces
    for (int i = 0; i < len; i++)
    {
        if (str[i] == '\0')
        {
            memmove(str + i, str + i + 1, len - i); // Shift characters left
            len--;                                  // Reduce the length of the string
            i--;                                    // Check the current position again
        }
    }
}

char *mergeString(char **tokenArr, int startIndex, int endIndex, char flag)
{
    int totalLength = 0;
    for (int i = startIndex; i <= endIndex; i++)
    {
        if (tokenArr[i] != NULL)
        {
            totalLength += strlen(tokenArr[i]);
        }
    }

    // Allocate memory for the merged string
    char *mergedString;
    if (flag == '1')
    {
        totalLength = totalLength + getSize(tokenArr) - 2;
        mergedString = (char *)malloc((totalLength + 1) * sizeof(char));
    }
    else
    {
        mergedString = (char *)malloc((totalLength + 1) * sizeof(char));
    }

    if (mergedString == NULL)
    {
        return NULL; // Memory allocation failed
    }

    // Copy the strings into the merged string
    int currentPos = 0;
    for (int i = startIndex; i <= endIndex; i++)
    {
        if (tokenArr[i] != NULL)
        {
            strcpy(mergedString + currentPos, tokenArr[i]);
            currentPos += strlen(tokenArr[i]);
        }
        // currentPos += strlen(tokenArr[i]);

        if ((flag == '1') && i < endIndex) // Add a space if not the last token
        {
            mergedString[currentPos] = ' ';
            currentPos++;
        }
    }

    // Null-terminate the merged string
    mergedString[totalLength] = '\0';
    removeNullTerminators(mergedString);
    // FreeHeap:
    return mergedString;
}

void removeDoubleQuotes(char *str)
{
    if (str == NULL)
    {
        return; // Handle NULL input
    }

    int len = strlen(str);

    // Initialize two pointers for in-place removal
    int readPos = 0;  // Read position
    int writePos = 0; // Write position

    // Iterate through the string character by character
    while (readPos < len)
    {
        // If the current character is not a double quote, copy it to the write position
        if (str[readPos] != '"')
        {
            str[writePos] = str[readPos];
            writePos++;
        }
        readPos++;
    }

    // Null-terminate the modified string
    str[writePos] = '\0';
}

void removeDoubleQuotesArr(char **tokenPtrArr)
{
    if (tokenPtrArr == NULL)
    {
        return;
    }

    for (int i = 0; tokenPtrArr[i] != NULL; i++)
    {
        removeDoubleQuotes(tokenPtrArr[i]);
    }
}

void printTokens(char **tokenArr)
{
    // printf("PRINTING THE TOKENS \n");
    for (int i = 0; i < getSize(tokenArr); i++)
    {
        printf("%s\n", tokenArr[i]);
    }
}

char **mergeArraysIndex(char **arr1, char **arr2, size_t index)
{
    // Calculate the lengths of the input arrays
    size_t len1 = 0;
    while (arr1[len1] != NULL)
    {
        len1++;
    }

    size_t len2 = 0;
    while (arr2[len2] != NULL)
    {
        len2++;
    }

    // Calculate the total length of the merged array
    size_t totalLen = len1 + len2 - 1;

    // Allocate memory for the merged array
    char **mergedArray = (char **)malloc((totalLen + 1) * sizeof(char *));

    // Copy elements from arr1 up to the specified index
    for (size_t i = 0; i < index && i < len1; i++)
    {
        mergedArray[i] = strdup(arr1[i]);
    }

    // Copy elements from arr2 into the merged array
    for (size_t i = 0; i < len2; i++)
    {
        mergedArray[index + i] = strdup(arr2[i]);
    }

    // Copy the remaining elements from arr1 after the specified index
    for (size_t i = index + 1; i < len1; i++)
    {
        // mergedArray[len2 + i - index] = strdup(arr1[i]);
        mergedArray[len2 + i - 1] = strdup(arr1[i]);
    }

    // Null-terminate the merged array
    mergedArray[totalLen] = NULL;

    return mergedArray;
}

// FOLLOWING FUNCTIONS ARE FOR THE WILD CARDS ::

char **matchPattern(char *pattern)
{
    glob_t glob_result;
    int globStatus = glob(pattern, 0, NULL, &glob_result);

    if (globStatus != 0 || glob_result.gl_pathc == 0)
    {
        // No matches or an error occurred
        char **result = (char **)malloc(2 * sizeof(char *));
        result[0] = strdup(pattern);
        result[1] = NULL;
        return result;
    }

    // Allocate memory for the results, including one for the NULL terminator
    char **result = (char **)malloc((glob_result.gl_pathc + 1) * sizeof(char *));

    for (size_t i = 0; i < glob_result.gl_pathc; ++i)
    {
        result[i] = strdup(glob_result.gl_pathv[i]);
    }

    result[glob_result.gl_pathc] = NULL; // Null-terminate the array

    globfree(&glob_result);
    return result;
}

// this function is responsible for expanding the wild cards if it encounters in a string line :

bool isWildcard(char *token)
{
    // Iterate over each character in the token
    bool insideQuotes = false;
    bool insideSingleQuote = false;

    for (size_t i = 0; token[i] != '\0'; i++)
    {
        if ((token[i] == '"'))
        {
            insideQuotes = !insideQuotes; // Toggle the insideQuotes flag
        }
        else if ((token[i] == '\''))
        {
            insideSingleQuote = !insideSingleQuote; // Toggle the insideQuotes flag
        }
        else if ((token[i] == '*' || token[i] == '?') && !insideQuotes && !insideSingleQuote)
        {
            return true; // Token contains a wildcard character
        }
    }
    return false; // Token does not contain a wildcard
}

char **expandedToken(char **tokenPtrArr)
{
    int arrSize = getSize(tokenPtrArr);
    char **newPtr = tokenPtrArr;
    for (int i = 0; i < arrSize; i++)
    {
        // printf("the array size is : %d \n", arrSize);
        if (i == arrSize)
        {
            break;
        }
        // printf("THE VALUE OF I : %d \n", i);
        // printTokens(tokenPtrArr);
        if (isWildcard(tokenPtrArr[i]))
        {
            // expand the token :
            char **newArr = matchPattern(tokenPtrArr[i]);
            // printf("THE SIZE OF THE WILD CARD IS %d \n", getSize(newArr));
            // printf("THE SIZE OF THE ORIGINAL ARRAY IS %d \n", arrSize);
            // printTokens(tokenPtrArr);
            char newSize = getSize(newArr);
            if (newSize == 1 && (strcmp(newArr[0], tokenPtrArr[i]) == 0))
            {
                continue;
            }
            else
            {
                // merge the array into the original at that index  ughh
                // printf("----\n");
                // printTokens(newArr);
                char **newPtrArr = mergeArraysIndex(newPtr, newArr, i);
                // printf("NEW ARRAY IS : \n");
                // printTokens(newPtrArr);
                free(newPtr);
                newPtr = newPtrArr;
                //
                tokenPtrArr = newPtr;
                //  i = i + newSize
                i = i + newSize - 1;
                arrSize = arrSize + newSize - 1;
            }
        }
    }
    return newPtr;
}

// Following function is just for the HISTORY, this will be a linklist type data structure :

struct HistoryNode
{
    char *enteredCmd;
    int index;
    struct HistoryNode *next;
};

void appendHistory(struct HistoryNode **historyList, char *cmd)
{
    struct HistoryNode *newNode = (struct HistoryNode *)malloc(sizeof(struct HistoryNode));
    if (newNode == NULL)
    {
        fprintf(stderr, "Memory allocation failed.\n");
        return;
    }

    newNode->enteredCmd = strdup(cmd);
    newNode->index = 0;

    newNode->next = NULL;

    if (*historyList == NULL)
    {
        *historyList = newNode;
        // printf("New index is : %d", newNode->index);
        return;
    }

    struct HistoryNode *temp = *historyList;
    while (temp->next != NULL)
    {
        temp = temp->next; // Traverse to the end of the list
    }

    newNode->index = temp->index + 1; // Increment the index for the new node
    temp->next = newNode;
}

void executeChaining(char *cmd, struct StringMap *aliasTable, struct HistoryNode **historyList, int isFork, int mainFD);

void printHistoryNode(struct HistoryNode **ptrHistoryList, int enteredIndex, struct StringMap *aliasTable)
{
    struct HistoryNode *historyList = *ptrHistoryList;
    enteredIndex -= 1;
    while (historyList != NULL && historyList->index != enteredIndex)
    {
        historyList = historyList->next;
    }
    if (historyList != NULL)
    {
        // printf("%s \n", historyList->enteredCmd);
        // creating tokens :
        // char **tokenPtrArr = getTokens(historyList->enteredCmd);
        executeChaining(historyList->enteredCmd, aliasTable, ptrHistoryList, 0, mainFD);
    }
    else
    {
        LOG_ERROR("Command is not found at this Index. \n");
        killMainProcess();
    }
}

void printAllHistory(struct HistoryNode *historyList)
{
    int counter = 1;
    while (historyList != NULL)
    {
        printf("%d %s \n", counter, historyList->enteredCmd);
        historyList = historyList->next;
        counter++;
    }
}

// FOLLOWING FUNCTIONS ARE FOR ALIAS & UNALIAS COMMAND ONLY :

void initStringMap(struct StringMap *map)
{
    map->data = NULL;
    map->size = 0;
}

void addToMap(struct StringMap *map, char *key, char *value)
{
    map->size++;
    // Reallocate memory for the data array
    map->data = (struct KeyValuePair *)realloc(map->data, map->size * sizeof(struct KeyValuePair));

    // Allocate memory for the key and value and copy the strings
    map->data[map->size - 1].key = strdup(key);
    map->data[map->size - 1].value = strdup(value);
    // printf("The new key map size is : %ld \n", map->size);
}

char *getValue(struct StringMap *map, char *key)
{

    for (size_t i = 0; i < map->size; i++)
    {
        if (strcmp(map->data[i].key, key) == 0)
        {
            return map->data[i].value;
        }
    }
    return NULL; // Key not found
}

void deleteToMap(struct StringMap *map, char *key)
{
    int found = -1; // Initialize a flag to indicate whether the key was found

    for (size_t i = 0; i < map->size; i++)
    {
        if (strcmp(map->data[i].key, key) == 0)
        {
            found = i;
            break;
        }
    }
    if (found != -1)
    {

        free(map->data[found].key);
        free(map->data[found].value);
        for (size_t i = found; i < map->size - 1; i++)
        {
            map->data[i] = map->data[i + 1];
        }
        map->size--;
        map->data = (struct KeyValuePair *)realloc(map->data, map->size * sizeof(struct KeyValuePair));
    }
}

void freeStringMap(struct StringMap *map)
{
    for (size_t i = 0; i < map->size; i++)
    {
        free(map->data[i].key);
        free(map->data[i].value);
    }
    free(map->data);
}

void printAliasTable(struct StringMap *map)
{
    // printf("The size is : %ld \n", map->size);
    for (size_t i = 0; i < map->size; i++)
    {
        printf("%s='%s'\n", map->data[i].key, map->data[i].value);
    }
}
// FOLLOWING FUNCITONS ARE CMD EXECUTION BUILT-IN FUNCTIONS

// function to execute the command :
void executels(char **tokenArr, int type)
{
    type = type + 1;
    int rc = fork();
    if (rc < 0)
    {
        LOG_ERROR("Failed to run the Fork \n");
    }
    else if (rc == 0)
    {
        // printf("THE TOKEN ARRAY IS : \n");
        // printTokens(tokenArr);
        char **trimArr = trimArrayNull(tokenArr, 0, getSize(tokenArr));
        // fflush(stdout);
        execvp(trimArr[0], trimArr);
        LOG_ERROR("Failed to execute the command: %s\n", trimArr[0]);
        exit(1);
    }
    else
    {

        int status;
        // pid_t child_pid = waitpid(rc, &status, 0);
        waitpid(rc, &status, 0);
        // LOG_DEBUG("p-id of the process is : %d \n", child_pid);
        if (isFork)
        {
            if (status == 0)
            {
                exit(0);
            }
            else
            {
                exit(1);
            }
        }
        else
        {
            // printf("the status was %d \n", status);
            childStatus = status;
        }
    }
}

void executePwd()
{
    char pwd[10000];
    if (getcwd(pwd, sizeof(pwd)) != NULL)
    {
        printf("%s\n", pwd);
    }
    else
    {
        LOG_ERROR("Error occured in executing the PWD command.");
    }
}

void executecd(char **tokenArr)
{

    // need to ensure 3 cases : 1) cd 2) cd /.... 3) cd .../..../

    if (getSize(tokenArr) == 1)
    {
        int status = chdir("/");
        if (status == -1)
        {
            LOG_ERROR("Unable to go to home Directory.\n");
            killMainProcess();
        }
    }
    else if (getSize(tokenArr) == 2)
    {
        char *gdir;
        char *dir;
        char *to;
        char buf[1024];
        gdir = getcwd(buf, sizeof(buf));
        if (gdir != NULL)
        {
            dir = strcat(gdir, "/");
            to = strcat(dir, tokenArr[1]);
            int status = chdir(to);
            if (status == -1)
            {
                LOG_ERROR("Incorrect Directory.\n");
                killMainProcess();
            }
        }
        else
        {
            LOG_ERROR("Incorrect Directory.\n");
            killMainProcess();
        }
    }
    else
    {
        LOG_ERROR("Incorrect Number of Arguments Provided to the cwd\n");
        killMainProcess();
    }
}

void executeAlias(char **tokenArr, struct StringMap *aliasTable)
{

    if (getSize(tokenArr) == 1)
    {
        printAliasTable(aliasTable);
    }
    else if (getSize(tokenArr) == 2)
    {
        char *aliasValue = getValue(aliasTable, tokenArr[1]);
        if (aliasValue == NULL)
        {
            LOG_ERROR("Alias value is not found \n");
            killMainProcess();
        }
        else
        {
            printf("%s='%s'\n", tokenArr[1], aliasValue);
        }
    }
    else if (getSize(tokenArr) == 3)
    {

        deleteToMap(aliasTable, tokenArr[1]);
        char *aliasKey = tokenArr[1];
        char *aliasValue = mergeString(tokenArr, 2, getSize(tokenArr) - 1, '1');
        removeDoubleQuotes(aliasValue);
        addToMap(aliasTable, aliasKey, aliasValue);
    }
    else
    {
        LOG_ERROR("Incorrect number of argumeents passed to the Alias cmd\n");
        killMainProcess();
    }
}

void executeEcho(char **tokenArr)
{
    char *echoString = mergeString(tokenArr, 1, getSize(tokenArr) - 1, '1');
    removeDoubleQuotes(echoString);
    printf("%s", echoString);
    printf("\n");
    fflush(stdout);
}

void checkCmdType(char **tokenArr, struct StringMap *aliasTable, struct HistoryNode **historyList, int type)
{

    char **targetTokenArr = NULL;
    char *aliasValue = getValue(aliasTable, tokenArr[0]);
    if (aliasValue == NULL)
    {
        targetTokenArr = tokenArr;
    }
    else
    {

        char **targetToken1Arr = getTokens(aliasValue);
        // re join the original passed array if the args are provided for it :
        if (tokenArr[1] != NULL)
        {

            char **originalTrimedArr = trimArrayNull(tokenArr, 1, getSize(tokenArr));
            targetTokenArr = mergeStringArrays(targetToken1Arr, originalTrimedArr);
            free(originalTrimedArr);
            free(targetToken1Arr);
        }
        else
        {
            targetTokenArr = targetToken1Arr;
        }
    }

    if (!(strcmp(targetTokenArr[0], "ls")))
    {
        executels(targetTokenArr, type);
    }
    else if (!(strcmp(targetTokenArr[0], "pwd")))
    {
        executePwd();
    }
    else if (!(strcmp(targetTokenArr[0], "cd")))
    {
        executecd(targetTokenArr);
    }
    else if (!(strcmp(targetTokenArr[0], "echo")))
    {
        // make function for the echo
        executeEcho(targetTokenArr);
    }
    else if (!(strcmp(targetTokenArr[0], "alias")))
    {
        // make function for Alias :
        executeAlias(targetTokenArr, aliasTable);
    }
    else if (!(strcmp(targetTokenArr[0], "unalias")))
    {
        // make function for the UnAlias
        if (getSize(targetTokenArr) == 2)
        {
            deleteToMap(aliasTable, targetTokenArr[1]);
        }
        else
        {
            LOG_ERROR("Incorrect Number of Arguments Passed \n");
            killMainProcess();
        }
    }

    else if (!(strcmp(targetTokenArr[0], "exit")))
    {
        exit(0);
    }
    else if (!(strcmp(targetTokenArr[0], "history")))
    {

        int cmdSize = getSize(targetTokenArr);

        if (cmdSize == 1)
        {
            printAllHistory(*historyList);
        }
        else if (cmdSize == 2)
        {
            printHistoryNode(historyList, atoi(targetTokenArr[1]), aliasTable);
        }
        else
        {
            LOG_ERROR("Incorrect Number of arguments passed to history. \n");
            killMainProcess();
        }
    }
    else
    {
        // merge the tokens to a string , remove double quotations , re-token them :
        executels(targetTokenArr, type);
    }
}

// FOLLOWING FUNCTIONS ARE FOR OPERATORS :

struct OperatorInfo
{
    char type;
    int index;
};

struct OperatorInfo detectOperator(char **tokenArr)
{

    const char *inputC = "<";
    const char *outputC = ">";
    const char *appendC = ">>";

    struct OperatorInfo status = {'0', -1};

    for (int i = 0; i < getSize(tokenArr); i++)
    {
        if ((!strcmp(tokenArr[i], inputC)) == 1)
        {
            status.index = i;
            status.type = 'i';
            return status;
        }
        if ((!strcmp(tokenArr[i], outputC)) == 1)
        {
            status.index = i;
            status.type = 'o';
            return status;
        }
        if ((!strcmp(tokenArr[i], appendC)) == 1)
        {
            status.index = i;
            status.type = 'a';
            return status;
        }
    }

    return status;
}

// input means that you are reading the data from the file

void inputRedirection(char **tokenArr, char *fileName, struct StringMap *aliasTable, struct HistoryNode **historyList)
{

    int rc = fork();
    if (rc < 0)
    {
        LOG_DEBUG("fork failed\n");
    }
    else if (rc == 0)
    {

        // changing the status of fork :
        int original_stdin = dup(STDIN_FILENO);
        isFork = true;
        // closeAllFileDescriptors(3);
        close(STDIN_FILENO);
        close(3);
        // closeAllFileDescriptors(3);
        int fd = open(fileName, O_RDONLY);

        if (fd == -1)
        {
            dup2(original_stdin, STDIN_FILENO);
            LOG_ERROR("Invalid File %s given out for redirection. \n", fileName);
            exit(EXIT_FAILURE);
        }

        char **newTrimArr = trimArrayNull(tokenArr, 0, 1);
        // char *args[] = {newTrimArr[0], NULL};
        // FreeHeap newTrimArr in this scope :
        checkCmdType(newTrimArr, aliasTable, historyList, 1);
        close(fd);
        exit(0);
    }
    else
    {
        // closeAllFileDescriptors(3);
        // wait(NULL);
        int status;
        // pid_t child_pid = waitpid(rc, &status, 0);
        waitpid(rc, &status, 0);
        if (isFork)
        {
            if (status == 0)
            {
                exit(0);
            }
            else
            {
                exit(1);
            }
        }
        else
        {
            childStatus = status;
        }
    }
}

void outputRedirection(char **tokenArr, char *fileName, struct StringMap *aliasTable, struct HistoryNode **historyList)
{
    // printf("HERE IN OUTPUT");
    // util_fd();
    int rc = fork();
    if (rc < 0)
    { // fork failed; exit
        fprintf(stderr, "fork failed\n");
        exit(1);
    }
    else if (rc == 0)
    {
        // child: redirect standard output to a file
        // closeAllFileDescriptors(3);
        int original_stdin = dup(STDIN_FILENO);
        isFork = true;
        close(STDOUT_FILENO);
        close(3);

        int fd = open(fileName, O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU);
        if (fd == -1)
        {
            dup2(original_stdin, STDIN_FILENO);
            LOG_ERROR("Invalid File %s given out for redirection. \n", fileName);
            exit(EXIT_FAILURE);
        }

        checkCmdType(tokenArr, aliasTable, historyList, 1);
        close(fd);
        exit(0);
    }
    else
    {
        // wait(NULL);
        int status;
        // pid_t child_pid = waitpid(rc, &status, 0);
        waitpid(rc, &status, 0);
        if (isFork)
        {
            if (status == 0)
            {
                exit(0);
            }
            else
            {
                exit(1);
            }
        }
        else
        {
            childStatus = status;
        }
    }
}

void appendRedirection(char **tokenArr, char *fileName, struct StringMap *aliasTable, struct HistoryNode **historyList)
{

    int rc = fork();
    if (rc < 0)
    { // fork failed; exit
        fprintf(stderr, "fork failed\n");
        exit(1);
    }
    else if (rc == 0)
    {
        int original_stdin = dup(STDIN_FILENO);
        isFork = true;
        close(STDOUT_FILENO);
        close(3);
        int fd = open(fileName, O_CREAT | O_WRONLY | O_APPEND, S_IRWXU);
        if (fd == -1)
        {
            dup2(original_stdin, STDIN_FILENO);
            LOG_ERROR("Invalid File %s given out for redirection. \n", fileName);
            exit(EXIT_FAILURE);
        }

        checkCmdType(tokenArr, aliasTable, historyList, 1);
        close(fd);
        exit(0);
    }
    else
    {
        // wait(NULL);
        int status;
        // pid_t child_pid = waitpid(rc, &status, 0);
        waitpid(rc, &status, 0);
        if (isFork)
        {
            if (status == 0)
            {
                exit(0);
            }
            else
            {
                exit(1);
            }
        }
        else
        {
            childStatus = status;
        }
    }
}

// variables needed : bool isFork ** int * status
void checkOperator(char **tokenArr, struct StringMap *aliasTable, struct HistoryNode **historyList, int flag)
{
    // Detect each operator :

    flag = flag + 1;
    struct OperatorInfo status = detectOperator(tokenArr);

    if (status.index == -1)
    {
        // or execute the normal instruction
        checkCmdType(tokenArr, aliasTable, historyList, 0);
        // printf("YESS HEREEEEEE\n");
    }
    else
    {
        // trim the array into 2 parts, the file & cmd , and pass both to the operator

        // call function for each specific type :

        char **execCmd = trimArrayNull(tokenArr, 0, status.index - 1);
        char *fileName = tokenArr[status.index + 1];
        // printf(" hello hamza \n");
        if (status.type == 'i')
        {
            inputRedirection(execCmd, fileName, aliasTable, historyList);
        }
        else if (status.type == 'o')
        {
            // printf("yes in here ! \n");
            outputRedirection(execCmd, fileName, aliasTable, historyList);
        }
        else
        {
            appendRedirection(execCmd, fileName, aliasTable, historyList);
        }

        /// FREEHEAP : execCmd in this scope
        free(execCmd);
    }

    // if (flag)
    // {
    //     exit(0);
    // }
}

// FOLLOWING FUNCTIONS ARE FOR PIPELINING :

// if you provide with the array : else if string then usin the func below
int getPipeNumber(char **tokenArr)
{
    int pipeCount = 0;
    char *pipChar = "|";
    for (int i = 0; i < getSize(tokenArr); i++)
    {

        if ((strcmp(tokenArr[i], pipChar)) == 1 && (strcmp(tokenArr[i + 1], pipChar)) == 1)
        {
            pipeCount += 1;
        }
    }

    return pipeCount;
}

int getPipeCount(char *inputString)
{

    int pipeCount = 0;
    bool insideQuotes = false;
    bool insideSingleQuote = false;
    // bool encounteredSinglePipe = false;

    for (char *c = inputString; *c != '\0'; c++)
    {
        if ((*c == '"'))
        {
            insideQuotes = !insideQuotes; // Toggle the insideQuotes flag
        }
        else if ((*c == '\''))
        {
            insideSingleQuote = !insideSingleQuote; // Toggle the insideQuotes flag
        }
        else if (*c == '|' && *(c + 1) != '|' && *(c - 1) != '|' && !insideQuotes && !insideSingleQuote)
        {
            pipeCount++;
        }
    }

    return pipeCount;
}

int **createPipeArr(int pipeCount)
{

    int **pipePtr = (int **)malloc((pipeCount + 1) * sizeof(int *));
    if (pipePtr == NULL)
    {
        return NULL;
    }

    for (int i = 0; i <= pipeCount; i++)
    {
        pipePtr[i] = (int *)malloc(2 * sizeof(int));
    }

    return pipePtr;
}

int activatePipes(int **pipeArr, int pipeCount)
{
    // Create pipes based on the pipeArr
    for (int i = 0; i <= pipeCount; i++)
    {
        if (pipe(pipeArr[i]) == -1)
        {
            perror("pipe");
            return 1; // Pipe creation failed
        }
    }

    return 0; // Pipes created successfully
}

// reading pipe -> from where i can read
// writing pipe -> on which i can write the output

void closePipes(int **pipeArr, int pipeCount, int index)
{

    for (int i = 0; i <= pipeCount; i++)
    {
        // for the first process aka the leftmost process in the pipe
        if (index == 0)
        {
            if (i == 0 && i == index)
            {
                // close the reading end pipe :
                close(pipeArr[i][0]);
            }
            else
            {
                close(pipeArr[i][0]);
                close(pipeArr[i][1]);
            }
        }
        // for intermediate processes in the pipes :
        else if (index != pipeCount && index != 0)
        {
            if (i == index)
            {
                // close the reading pipe of your slot as you will only write to your
                close(pipeArr[i][0]);
            }
            else if (i == index - 1)
            {
                // dont close the reading pipe of the command from where you are taking the input so close its writing :
                close(pipeArr[i][1]);
            }
            else
            {
                // close both the reading & writing pipes of others commands :
                close(pipeArr[i][0]);
                close(pipeArr[i][1]);
            }
        }
        // for the last processes aka the right most ending cmd in the pipeline :
        else if (index == pipeCount)
        {
            if (i == (pipeCount - 1))
            {
                // you just need to read from the previous command output & thats it so just close the writing pipe of the other :
                close(pipeArr[i][1]);
            }
            else
            {
                close(pipeArr[i][0]);
                close(pipeArr[i][1]);
            }
        }
    }
}

bool isWhitespace(char c)
{
    return c == '\t' || c == '\n';
}

char **pipeTokens(char *inputLine)
{

    char **tokens = NULL;
    size_t tokenCount = 0;
    char token[MAX_TOKEN_LENGTH] = "";
    size_t tokenIndex = 0;
    bool insideQuotes = false;
    bool insideSingleQuotes = false;

    for (const char *p = inputLine; *p != '\0'; p++)
    {

        if (*p == '\'')
        {
            insideSingleQuotes = !insideSingleQuotes;
        }

        if (*p == '"')
        {
            insideQuotes = !insideQuotes; // Toggle the insideQuotes flag when a double quote is encountered
        }
        // else if (*p == '\'')
        // {
        //     insideSingleQuotes = !insideSingleQuotes;
        // }

        else if ((*p == '|' && !insideQuotes && !insideSingleQuotes && !(*p == '\'') && (*(p - 1) != '|' && *(p + 1) != '|')))
        {
            if (tokenIndex > 0)
            {
                token[tokenIndex] = '\0';
                tokenIndex = 0;
                tokenCount++;
                tokens = realloc(tokens, (tokenCount + 1) * sizeof(char *));
                tokens[tokenCount - 1] = strdup(token);
            }
        }
        else if (!isWhitespace(*p) || insideQuotes || *p == '\'')
        {
            token[tokenIndex++] = *p;
        }
    }

    if (tokenIndex > 0)
    {
        token[tokenIndex] = '\0';
        tokenCount++;
        tokens = realloc(tokens, (tokenCount + 1) * sizeof(char *));
        tokens[tokenCount - 1] = strdup(token);
    }

    tokens[tokenCount] = NULL; // Null-terminate the array

    return tokens;
}

void startRecursionPiping(char **commands, int **pipeArrays, int myIndex, int originalPipeNumb, struct StringMap *aliasTable, struct HistoryNode **historyList)
{
    // if you are the first command
    // printf("mmy index : index = %d : total pipes = %d \n", myIndex, originalPipeNumb);
    // aka the left most command which just need to send its output to the write channel : && no need to create forks .
    if (myIndex == 0)
    {

        closePipes(pipeArrays, originalPipeNumb, myIndex);
        dup2(pipeArrays[myIndex][1], STDOUT_FD);
        // removeDoubleQuotes(commands[myIndex]);
        char **tokens = getTokens(commands[myIndex]);
        tokens = expandedToken(tokens);
        removeDoubleQuotesArr(tokens);
        checkOperator(tokens, aliasTable, historyList, 1);

        // executeChaining(commands[myIndex], aliasTable, historyList, false);
        exit(0);
    }
    else
    {
        // create forks now to let other processes spawn to execute the commands from the left hand side of the pipeline to right hand side
        int rc1 = fork();
        if (rc1 < 0)
        {
            LOG_DEBUG("fork failed\n");
        }
        // child process :
        else if (rc1 == 0)
        {
            isFork = true;
            startRecursionPiping(commands, pipeArrays, myIndex - 1, originalPipeNumb, aliasTable, historyList);
            exit(0);
        }
        // for the parent :
        else
        {

            closePipes(pipeArrays, originalPipeNumb, myIndex);

            // wait(NULL);

            int status;
            // pid_t child_pid = waitpid(rc1, &status, 0);
            waitpid(rc1, &status, 0);
            if (isFork && status != 0)
            {
                // printf("i am failed :() \n");
                exit(1);
            }

            if ((myIndex < originalPipeNumb))
            {
                // printf("yesss!!!4545! \n");
                // closePipes(pipeArrays, originalPipeNumb, myIndex);
                dup2(pipeArrays[myIndex][1], STDOUT_FD);
                dup2(pipeArrays[myIndex - 1][0], STDIN_FD);
                // closeAllFileDescriptors(3);

                // executeChaining(commands[myIndex], aliasTable, historyList, false);

                // removeDoubleQuotes(commands[myIndex]);
                char **tokens = getTokens(commands[myIndex]);
                tokens = expandedToken(tokens);
                checkOperator(tokens, aliasTable, historyList, 1);
                removeDoubleQuotesArr(tokens);
                // closeAllFileDescriptors(0);
                close(pipeArrays[myIndex][1]);
                close(pipeArrays[myIndex - 1][0]);
                if (isFork)
                {
                    if (status == 0)
                    {
                        // printf("Success \n");
                        exit(0);
                    }
                    else
                    {
                        // printf("Failure \n");
                        exit(1);
                    }
                }
                else
                {
                    childStatus = status;
                }
                // exit(0);
            }
            // if this is the rightmost output cmd :
            else
            {
                int original_stdin = dup(STDIN_FILENO);
                // closePipes(pipeArrays, originalPipeNumb, myIndex);
                dup2(pipeArrays[myIndex - 1][0], STDIN_FD);
                // closeAllFileDescriptors(4);
                // executeChaining(commands[myIndex], aliasTable, historyList, false);
                // removeDoubleQuotes(commands[myIndex]);
                char **tokens = getTokens(commands[myIndex]);
                tokens = expandedToken(tokens);
                removeDoubleQuotesArr(tokens);
                // // printTokens(tokens);

                checkOperator(tokens, aliasTable, historyList, 0);

                dup2(original_stdin, STDIN_FILENO);

                close(pipeArrays[myIndex - 1][0]);
                // util_fd();
                if (isFork)
                {
                    if (status == 0)
                    {
                        exit(0);
                    }
                    else
                    {
                        exit(1);
                    }
                }
                else
                {
                    // printf("the status : %d \n", status);
                    childStatus = status;
                }

                // exit(0);

                // dup2(STDIN_FD, STDIN_FILENO);
            }
        }
    }
}

bool checkPipeValidity(char **commands)
{
    for (int i = 0; i < getSize(commands); i++)
    {
        if (i == 0)
        {
            for (char *p = commands[i]; *p != '\0'; p++)
            {
                if (*p == '>')
                {
                    return false;
                }
            }
        }
        else if (i > 0 && i < (getSize(commands) - 1))
        {
            for (char *p = commands[i]; *p != '\0'; p++)
            {
                if (*p == '>' || *p == '<')
                {
                    return false;
                }
            }
        }
        else if (i == getSize(commands))
        {
            for (char *p = commands[i]; *p != '\0'; p++)
            {
                if (*p == '<')
                {
                    return false;
                }
            }
        }
    }

    return true;
}

void initiatePiping(char *inputString, struct StringMap *aliasTable, struct HistoryNode **historyList, int fileFD)
{
    int maxCount = getPipeCount(inputString);
    // printf("number of pipes :%d \n", maxCount);
    if (maxCount == 0)
    {
        printf("Zero piping!\n"); // Execute the cmd normally ..
    }
    else
    {

        // before launching the piping check the validity of the command :
        char **commands = pipeTokens(inputString);
        bool pipeFlag = checkPipeValidity(commands);

        if (pipeFlag == false)
        {
            LOG_ERROR("The pipe command is not adhering to the appropriate I/O redirection \n");
            killMainProcess();
        }

        int rc1 = fork();
        if (rc1 < 0)
        {
            LOG_DEBUG("fork failed\n");
        }
        // child process :
        else if (rc1 == 0)
        {

            char **commands = pipeTokens(inputString);
            // printf("the number of tokens : %d \n", getSize(commands));
            // printTokens(commands);
            int **pipeArrays = createPipeArr(maxCount);
            activatePipes(pipeArrays, maxCount);
            if (fileFD > 0)
            {
                close(fileFD);
            }
            // activatePipes(pipeArrays, maxCount);
            isFork = true;
            startRecursionPiping(commands, pipeArrays, maxCount, maxCount, aliasTable, historyList);
        }
        else
        {
            // closeAllFileDescriptors(4);
            // wait(NULL);

            int status;
            // pid_t child_pid = waitpid(rc1, &status, 0);
            waitpid(rc1, &status, 0);
            if (isFork)
            {
                if (status == 0)
                {
                    exit(0);
                }
                else
                {
                    exit(1);
                }
            }
            else
            {
                // printf("the status of the execution of piping iss : %d \n", status);
                childStatus = status;
                // if (status != 0)
                // {
                //     printf("The pipeline command failed to execute. \n");
                // }
            }
        }
    }
}

// FOLLOWING FUNCTIONS ARE JUST FOR CHAINING :

// creater a parser based on the operators

// recursively launch the stuff

// store the state of the operators

// manage the pipes ::

int getOperatorCount(char *inputString)
{
    int operatorCount = 0;
    bool insideQuotes = false;
    bool insideSingleQuote = false;

    for (char *c = inputString; *c != '\0'; c++)
    {
        if (*c == '"')
        {
            insideQuotes = !insideQuotes; // Toggle the insideQuotes flag
        }
        else if (*c == '\'')
        {
            insideSingleQuote = !insideSingleQuote; // Toggle the insideSingleQuote flag
        }
        else if (!insideQuotes && !insideSingleQuote)
        {
            if ((c[0] == '&' && c[1] == '&') || (c[0] == '|' && c[1] == '|') || c[0] == ';')
            {
                operatorCount++;
                c++; // Skip the second character of the operator
            }
        }
    }

    return operatorCount;
}

bool isOperatorWhitespace(char c)
{
    return (c == '\t' || c == '\n' || c == '\r');
}

char **getOperatorTokens(char *inputLine)
{
    char **tokens = NULL;
    size_t tokenCount = 0;
    char token[MAX_TOKEN_LENGTH] = "";
    size_t tokenIndex = 0;
    bool insideQuotes = false;
    bool insideSingleQuotes = false;
    bool operatorDetected = false;

    for (const char *p = inputLine; *p != '\0'; p++)
    {

        if (*p == '\'')
        {
            insideSingleQuotes = !insideSingleQuotes;
        }

        if (*p == '"')
        {
            insideQuotes = !insideQuotes;
        }
        if (!insideQuotes && !insideSingleQuotes)
        {
            if (strncmp(p, "&&", 2) == 0 || strncmp(p, "||", 2) == 0)
            {
                if (tokenIndex > 0)
                {
                    token[tokenIndex] = '\0';
                    tokenIndex = 0;
                    tokenCount++;
                    tokens = realloc(tokens, (tokenCount + 1) * sizeof(char *));
                    tokens[tokenCount - 1] = strdup(token);
                }
                operatorDetected = true;
                p++; // Skip the second character of the special token
            }
            else if (*p == ';')
            {
                if (tokenIndex > 0)
                {
                    token[tokenIndex] = '\0';
                    tokenIndex = 0;
                    tokenCount++;
                    tokens = realloc(tokens, (tokenCount + 1) * sizeof(char *));
                    tokens[tokenCount - 1] = strdup(token);
                }
                operatorDetected = true;
            }
        }

        if ((!isOperatorWhitespace(*p) || insideQuotes || *p == '\'') && !operatorDetected)
        {
            token[tokenIndex++] = *p;
        }
        if (operatorDetected)
        {
            operatorDetected = false;
        }
    }

    if (tokenIndex > 0)
    {
        token[tokenIndex] = '\0';
        tokenCount++;
        tokens = realloc(tokens, (tokenCount + 1) * sizeof(char *));
        tokens[tokenCount - 1] = strdup(token);
    }

    tokens[tokenCount] = NULL; // Null-terminate the array
    return tokens;
}

char **getOperatorArr(char *inputLine)
{
    char **operators = NULL;
    int operatorCount = 0;
    bool insideQuotes = false;
    bool insideSingleQuote = false;

    for (char *c = inputLine; *c != '\0'; c++)
    {
        if (*c == '"')
        {
            insideQuotes = !insideQuotes; // Toggle the insideQuotes flag
        }
        else if (*c == '\'')
        {
            insideSingleQuote = !insideSingleQuote; // Toggle the insideSingleQuote flag
        }
        else if (!insideQuotes && !insideSingleQuote)
        {
            if ((c[0] == '&' && c[1] == '&') || (c[0] == '|' && c[1] == '|') || c[0] == ';')
            {
                (operatorCount)++;
                operators = realloc(operators, (operatorCount) * sizeof(char *));
                operators[(operatorCount)-1] = malloc(3 * sizeof(char));
                strncpy(operators[(operatorCount)-1], c, 2);
                operators[(operatorCount)-1][2] = '\0';
                c++; // Skip the second character of the operator
            }
            else if (c[0] == ';')
            {
                (operatorCount)++;
                operators = realloc(operators, (operatorCount) * sizeof(char *));
                operators[(operatorCount)-1] = malloc(2 * sizeof(char)); // Allocate space for only the semicolon and null terminator
                strncpy(operators[(operatorCount)-1], c, 1);
                operators[(operatorCount)-1][1] = '\0';
            }
        }
    }

    operators[operatorCount] = NULL;
    return operators;
}

void executeChaining(char *line, struct StringMap *aliasTable, struct HistoryNode **historyList, int flag, int fileFD)
{
    // tokenize the string based on the entered the commands :

    int countOperator = getOperatorCount(line);
    if (countOperator == 0)
    {
        // execute the commmand normally ...
        // removeDoubleQuotes(line);
        char **tokenPtrArr = getTokens(line);
        tokenPtrArr = expandedToken(tokenPtrArr);
        removeDoubleQuotesArr(tokenPtrArr);
        checkOperator(tokenPtrArr, aliasTable, historyList, flag);
    }
    else
    {
        // initialze the tokens , index , operatorArrays
        char **chainingOperatorArrays = getOperatorTokens(line);
        // token Array :
        char **operators = getOperatorArr(line);
        // printf("yesss! \n");
        // printTokens(operators);
        //  printf("The operator commands array : \n");
        //  printTokens(chainingOperatorArrays);
        //  printf("All operators : \n");
        //  // printTokens(operators);
        //  int executionStatus = 0;
        //  Make a for loop that will execute this cmd :
        for (int i = 0; i <= countOperator; i++)
        {
            if (getPipeCount(chainingOperatorArrays[i]))
            {
                initiatePiping(chainingOperatorArrays[i], aliasTable, historyList, fileFD);
            }
            else
            {
                // removeDoubleQuotes(chainingOperatorArrays[i]);
                char **tokenPtrArr = getTokens(chainingOperatorArrays[i]);
                tokenPtrArr = expandedToken(tokenPtrArr);
                removeDoubleQuotesArr(tokenPtrArr);
                checkOperator(tokenPtrArr, aliasTable, historyList, 1);
            }

            // int status = checkOperator(tokenPtrArr , aliasTable , historyList , flag);
            /*
                if status this and that do this and that ....
            */
            // this is the final line of execution in the chaining;
            if (i >= countOperator)
            {
                break;
            }
            // printf("THE EXECUTION STATUS OF PREVIOUS COMMAND IS : %d \n", childStatus);
            if (childStatus == 0)
            {
                if (strcmp(operators[i], "&&") == 0)
                {
                    // execute next cmd
                    continue;
                }
                else if (strcmp(operators[i], "||") == 0)
                {
                    // skip next cmd
                    i++;
                }
                else
                {
                    // for semi colon , execute next cmd ...
                    continue;
                }
            }
            else
            {
                if (strcmp(operators[i], "&&") == 0)
                {
                    // skip next cmd
                    i++;
                }
                else if (strcmp(operators[i], "||") == 0)
                {
                    // continue next cmd
                    continue;
                }
                else
                {
                    // for semi colon , execute next cmd ...
                    continue;
                }
            }
        }
    }
}

// all the main brain of Recursions will happen over here :

int main(int argc, char **argv)
{

    // The Main Loop :

    // printf("the number of params %d , %s \n", argc, argv[0]);

    // creating Alias table :
    // printf("Script mode started \n");
    struct StringMap *aliasTable = (struct StringMap *)malloc(sizeof(struct StringMap));
    // creating histroy pointer node :
    struct HistoryNode *historyList = NULL;
    // FreeHeap    Alias table :
    if (aliasTable == NULL)
    {
        // Use aliasTable for your operations.
        // When you're done with aliasTable, don't forget to free the memory:
        free(aliasTable);
        return 0;
    }

    initStringMap(aliasTable);

    // Switch to interactive vs Script mode :
    int fileFD;
    // printf("THE VALUE OF ARGC %d", argc);
    if (argc > 1)
    {
        fileFD = 3;
        mainFD = fileFD;
        FILE *file = fopen(argv[1], "r");
        // FILE *file = fopen("./test/Tests/quotes.test", "r");

        // printf("After OPENING FILE \n");
        // util_fd();
        // Check if the file opened successfully
        if (file == NULL)
        {
            perror("Error opening file");
            return 1; // Exit with an error code
        }
        // Buffer to store each line

        char line[2048]; // Adjust the size as needed

        int x = 0;
        // printf("Script mode started \n");
        //  Read and print each line
        while (fgets(line, sizeof(line), file))
        {
            x++;
            // printf("Line:\n");
            // removin the newline character if its present
            if (line[strlen(line) - 1] == '\n')
            {
                line[strlen(line) - 1] = '\0';
            }
            // printf("LINEX EXTRACTED %s : %d : \n", line, x);
            //  checkCmdType(tokenPtrArr, aliasTable, &historyList);

            // if (getPipeCount(line))
            // {
            //     initiatePiping(line, aliasTable, &historyList, fileFD);
            // }
            if (strlen(line) > 0)
            {
                add_history(line);
                if (getOperatorCount(line))
                {
                    // printf("THE OPERATOR COUNT : %d \n", getOperatorCount(line));
                    // char **operatorTokens = getOperatorTokens(line);
                    // printTokens(operatorTokens);
                    executeChaining(line, aliasTable, &historyList, false, fileFD);
                }
                else if (getPipeCount(line))
                {
                    initiatePiping(line, aliasTable, &historyList, fileFD);
                }
                else
                {

                    // removeDoubleQuotes(line);
                    char **tokenPtrArr = getTokens(line);
                    tokenPtrArr = expandedToken(tokenPtrArr);
                    removeDoubleQuotesArr(tokenPtrArr);
                    checkOperator(tokenPtrArr, aliasTable, &historyList, 0);

                    free(tokenPtrArr);
                }
            }
            // Free the tokenPtrArr!!! ....
            // printf("Line: %s\n", line);
        }

        // Close the file when done
        fclose(file);
    }
    else
    {
        fileFD = -1;
        mainFD = fileFD;
        while (1)
        {
            // Get the input from the
            char *input = readline("prompt> ");

            if (strlen(input) == 0)
            {
                continue;
            }
            // adding the history :
            add_history(input);

            // free the input String perhaps
            // printf("the cmd given : %s \n", input);
            appendHistory(&historyList, input);

            // removeDoubleQuotes(input);
            // Check if the user entered EOF (Ctrl+D)
            if (!input)
            {
                printf("\nGoodbye!\n");
                break;
            }

            if (strlen(input) > 0)
            {
                if (getOperatorCount(input))
                {
                    // printf("THE OPERATOR COUNT : %d \n", getOperatorCount(line));
                    // char **operatorTokens = getOperatorTokens(line);
                    // printTokens(operatorTokens);
                    executeChaining(input, aliasTable, &historyList, false, fileFD);
                }
                else if (getPipeCount(input))
                {
                    initiatePiping(input, aliasTable, &historyList, fileFD);
                }
                else
                {

                    // removeDoubleQuotes(line);
                    char **tokenPtrArr = getTokens(input);
                    tokenPtrArr = expandedToken(tokenPtrArr);
                    removeDoubleQuotesArr(tokenPtrArr);
                    checkOperator(tokenPtrArr, aliasTable, &historyList, 0);

                    free(tokenPtrArr);
                }
            }
        }
    }

    return 0;
}
