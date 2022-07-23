// The MIT License (MIT)
//
// Copyright (c) 2020 Trevor Bakker
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

/*//////////////////////////////////////
|
|    Students in this group:
|    Jaedyn K. Brown  - Section 003
|    Robert Carr      - Section 002
|
*///////////////////////////////////////

#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <stdint.h>
#include <ctype.h>
#include <stdbool.h>
#define MAX_NUM_ARGUMENTS 4 // ***** 4 for read *****

#define WHITESPACE " \t\n"      // We want to split our command line up into tokens
                                // so we need to define what delimits our tokens.
                                // In this case  white space
                                // will separate the tokens on our command line
#define MAX_COMMAND_SIZE 255    // The maximum command-line size

struct __attribute__((__packed__)) DirectoryEntry
{//Each record can be represented by:
char DIR_Name[11];
uint8_t DIR_Attr;
uint8_t Unused1[8];
uint16_t DIR_FirstClusterHigh;
uint8_t Unused2[4];
uint16_t DIR_FirstClusterLow;
uint32_t DIR_FileSize;
};
struct DirectoryEntry dir[16];

// A global int value acting as a bool to keep
// track of whether or not a file is open
// 0 = not open, 1 = is open
int fileIsOpen = 0;
int afterClose = 0;

//laying out variables needed for FAT32 layout
//                      offset     size(bytes)
char BS_OEMName[8]; //     3,           8
int16_t BPB_BytsPerSec; // 11,          2
int8_t BPB_SecPerClus; //  13,          1
int16_t BPB_RsvdSecCnt; // 14,          2
int8_t BPB_NumFATs;//      16,          1
int16_t BPB_RootEntCnt;//  17,          2
char BS_VolLab[11];//      19,          2
int32_t BPB_FATSz32;//     36,          4 //starting at offset 36 of boot sector
int32_t BPB_RootClus;//    44,          4 //starting at offset 36 of boot sector
int32_t RootDirSectors = 0; // RootDirSectors = ((BPB_RootEntCnt * 32) + (BPB_BytsPerSec -1 )) / BPB_BytsPerSec
int32_t FirstDataSector = 0;
int32_t FirstSectorofCluster = 0;
//get root directory information, fseek to root directory address, loop from i = 0 to i < 16; fread 32 bytes into directory entry array

FILE *fp; //globally declare for function
int LBAToOffset(int32_t sector);
void decConverter( int decimal );

bool getterPrinter = false;

int main()
{
    char * cmd_str = (char*) malloc( MAX_COMMAND_SIZE );
    while( 1 )
    {
        // Print out the mfs prompt
        printf ("mfs> ");

        // Read the command from the commandline.  The
        // maximum command that will be read is MAX_COMMAND_SIZE
        // This while command will wait here until the user
        // inputs something since fgets returns NULL when there
        // is no input
        while( !fgets (cmd_str, MAX_COMMAND_SIZE, stdin) );

        /* Parse input */
        char *token[MAX_NUM_ARGUMENTS];
        int   token_count = 0;                          
                                             
        // Pointer to point to the token
        // parsed by strsep
        char *arg_ptr;                                  
                                             
        char *working_str  = strdup( cmd_str );         

        // we are going to move the working_str pointer so
        // keep track of its original value so we can deallocate
        // the correct amount at the end
        char *working_root = working_str;

        // Tokenize the input stringswith whitespace used as the delimiter
        while ( ( (arg_ptr = strsep(&working_str, WHITESPACE ) ) != NULL) &&
              (token_count<MAX_NUM_ARGUMENTS))
        {
            token[token_count] = strndup( arg_ptr, MAX_COMMAND_SIZE );
            if( strlen( token[token_count] ) == 0 )
            {
                token[token_count] = NULL;
            }
            token_count++;
        }

        // "filenames shall not contain spaces and shall be
        // limited to 100 characters
        char filename[100];
        char readFilename[100];
        
        if(token[0] == NULL) //Need a base case where it catches if there's nothing at token[0]
        {
            continue;
        }
        
        // Code for opening a FAT32 image
        if(strcmp(token[0], "open") == 0)
        {
            if(fileIsOpen == 1)
            {
                printf("Error: File system image already open.\n");
            }
            else
            {
                // Saving the user entered filename into filename
                strcpy(filename, token[1]);
                fp = fopen(filename, "r");
                if(fp == NULL)
                {
                    printf("Error: File system image not found.\n");
                }
                else
                {
                    fileIsOpen = 1;
                    if(afterClose == 1)
                    {
                        afterClose = 1;
                    }
                    else
                    {
                        afterClose = 0;
                    }
     
                    // We need to fill in our FAT variables here.
                    fseek(fp, 3, SEEK_SET); //move up 3 bytes for the fp file pointer
                    fread(&BS_OEMName, 8, 1, fp); //read 8 bytes, 1 element/ byte, from fp, into variable BPB_OEMName
                    fseek(fp, 11, SEEK_SET);
                    fread(&BPB_BytsPerSec, 2, 1, fp); //2 bytes
                    fread(&BPB_SecPerClus, 1, 1, fp); //1 byte
                    fread(&BPB_RsvdSecCnt, 2, 1, fp);
                    fread(&BPB_NumFATs, 1, 1, fp);
                    fread(&BPB_RootEntCnt, 2, 1, fp); //this is the last variable for the first cluster
                    fseek(fp, 36, SEEK_SET); //next cluster starts at offset 36
                    fread(&BPB_FATSz32, 4, 1, fp);
  
                    fseek(fp, 44, SEEK_SET);
                    fread(&BPB_RootClus, 4, 1, fp); // think this is the last variable that I have to fill for the FAT layout
                    FirstSectorofCluster = BPB_RootClus; //found sector number
                    
                    //finding the starting address of our block of data since we found sector number
                    int offsetMarker = LBAToOffset(FirstSectorofCluster);
                    fseek(fp, offsetMarker, SEEK_SET);
                    fread(&dir[0], 32, 16, fp); //16, 32 byte records
                }
            }
        }
        
        // Code for closing a FAT32 image
        if(strcmp(token[0], "close") == 0)
        {
            if(fileIsOpen == 1)
            {
                fclose(fp);
                printf("The file has been closed.\n");
                fileIsOpen = 0;
                afterClose = 1;
            }
            else
            {
                printf("Error: File system is not open.\n");
            }
        }
        
        // Code for printing out info about the filesystem
        if(strcmp(token[0], "info") == 0)//just need to print out all the variables attached to FAT32 layout, in dec. and hex.
        {
            if(fileIsOpen == 1)
            {
                // Do code here
                printf("BPB_BytsPerSec: \t%d\t", BPB_BytsPerSec);
                decConverter(BPB_BytsPerSec);
                printf("BPB_SecPerClus: \t%d\t", BPB_SecPerClus);
                decConverter(BPB_SecPerClus);
                printf("PBPB_RsvdSecCnt: \t%d\t", BPB_RsvdSecCnt);
                decConverter(BPB_RsvdSecCnt);
                printf("BPB_NumFATS:    \t%d\t", BPB_NumFATs);
                decConverter(BPB_NumFATs);
                printf("BPB_FATSz32:    \t%d\t", BPB_FATSz32);
                decConverter(BPB_FATSz32);
            }
            else
            {
                printf("Error: File system image must be opened first.\n");
            }
        }
        
        // Code for printing out attributes about the filesystem
        if(strcmp(token[0], "stat") == 0)
        {
            if(fileIsOpen == 1)
            {
                if(token[1] == NULL)
                {
                    printf("Please give a valid input. \n");
                }
                else //if condition above didn't go through, then we can assume the input was valid
                {
                    int offsetMarker = LBAToOffset(FirstSectorofCluster);//current directory
                    fseek(fp, offsetMarker, SEEK_SET);//move file pointer up to that offset
                    char expanded_name[12]; //following example in compare.c
                    char *cdArr = token[1]; //workable pointer to where we want to cd into
                    memset( expanded_name, ' ', 12);
                    char *token = strtok (cdArr, ".");
                    int matchFound = 0;
                    if(token != NULL)//need this or else we seg fault
                    {    
                        strncpy(expanded_name, token, strlen(token)); //store tokenized file into expanded_name
                        token = strtok( NULL, ".");
                        if(token)//compare.c
                        {
                            strncpy( (char *)(expanded_name+8), token, strlen(token));
                        }
                        expanded_name[11] = '\0';
                        int i;
                        for( i = 0; i < 11; i ++)//code in compare.c, convert user typed token[1] into uppercase, if not already
                        {    
                            //set user input to upper to match uppercase letters in files stored
                            expanded_name[i] = toupper( expanded_name[i] );
                        }
        
                        for(int i = 0; i <= 15; i++)//matching positions of dir[]
                        {
                            char *dirList = (char*)malloc(11); //directory name size 11
                            memset(dirList, ' ', 11);
                            memcpy(dirList, dir[i].DIR_Name, 11);//controlling bytes being compared with expanded_name
                            
                            if( strncmp(expanded_name, dirList, 11) == 0) //match found
                            {
                                printf("File Size: %d\n", dir[i].DIR_FileSize);
                                printf("First Cluster Low: %d\n", dir[i].DIR_FirstClusterLow);
                                printf("DIR_ATT: %u\n", dir[i].DIR_Attr);
                                printf("First Cluster High: %d\n", dir[i].DIR_FirstClusterHigh);
                                matchFound = 1;
                            }
                        }
                        if(matchFound == 0)
                        {
                            printf("Error: File not found.\n");
                        }
                    }
                }
            }
            else
            {
                printf("Error: File system image must be opened first.\n");
            }
        }

        // Code for getting a file from the filesystem
        if(strcmp(token[0], "get") == 0) // cluster
        {
            int store;
            int fileSize;
            if(fileIsOpen == 1)
            {
                int offsetMarker = LBAToOffset(FirstSectorofCluster);//current directory
                if(token[1] == NULL)
                {
                    printf("Please give a valid input. \n");
                }
                else//if condition above didn't go through, then we can assume the input was valid
                {
                    char expanded_name[12]; //following example in compare.c
                    char *readArr = token[1]; //workable pointer to where we want to read into
                    memset( expanded_name, ' ', 12);
                    strcpy(readFilename, token[1]);
                    char savedFileName[12];
                    strcpy(savedFileName, token[1]);
                    int matchFound = 0;
                    char *token = strtok (readArr, ".");
                    if(token != NULL)//need this or else we seg fault
                    {    
                        strncpy(expanded_name, token, strlen(token)); //store tokenized file into expanded_name
                        token = strtok( NULL, ".");
                        if(token)//compare.c
                        {
                            strncpy( (char *)(expanded_name+8), token, strlen(token));
                        }
             
                        expanded_name[11] = '\0';
                        int i;
             
                        for( i = 0; i < 11; i ++)//code in compare.c, convert user typed token[1] into uppercase, if not already
                        {    
                           //set user input to upper to match uppercase letters in files stored
                           expanded_name[i] = toupper( expanded_name[i] );
                        }
                    }
                    else
                    {
                        strncpy(expanded_name, readArr, strlen(readArr));
                        expanded_name[11] = '\0';
                    }
             
                    for(int i = 0; i <= 15; i++)//matching positions of dir[]
                    {
                        char *dirList = (char*)malloc(11); //directory name size 11
                        memset(dirList, ' ', 11);
                        memcpy(dirList, dir[i].DIR_Name, 11);//controlling bytes being compared with expanded_name
                  
                        if( strncmp(expanded_name, dirList, 11) == 0) //match found
                        {
                            store = dir[i].DIR_FirstClusterLow;//April 12th lecture 26:12
                            fileSize = dir[i].DIR_FileSize;
                            matchFound = 1;
                        }
                    }
          
                    if(matchFound == 0)
                    {
                        printf("Error: File not found.\n");
                    }
                    else
                    {  
                        FILE *ofp = fopen(savedFileName, "w");
                        fseek(fp, LBAToOffset(store), SEEK_SET);
                        // In office hours he set buffer to 512, but I did it to 1000 here for testing
                        unsigned char *buffer = (unsigned char*)malloc(fileSize);
                        fread(buffer, fileSize, 1, fp);
                        fwrite(buffer, fileSize, 1, ofp);
                        fclose(ofp);
                        getterPrinter = true;
                    }
                }
            }
            else
            {
                printf("Error: File system image must be opened first.\n");
            }
        }

        // Code for changing the current working directory
        if(strcmp(token[0], "cd") == 0)
        {
            int store;
            if(fileIsOpen == 1)
            {
                // Do code here
                if(token[1] == NULL)
                {
                   printf("Please give a valid input. \n");
                }
                else//if condition above didn't go through, then we can assume the input was valid
                {
                    char expanded_name[12]; //following example in compare.c
                    char *cdArr = token[1]; //workable pointer to where we want to cd into
                    memset( expanded_name, ' ', 12);
                    char *token = strtok (cdArr, ".");
                    if(token != NULL)//need this or else we seg fault
                    {
                        strncpy(expanded_name, token, strlen(token)); //store tokenized file into expanded_name
                        token = strtok( NULL, ".");
                        if(token)//compare.c
                        {
                            strncpy( (char *)(expanded_name+8), token, strlen(token));
                        }
                        expanded_name[11] = '\0';
                        int i;
                        for( i = 0; i < 11; i ++)//code in compare.c, convert user typed token[1] into uppercase, if not already
                        {
                            //set user input to upper to match uppercase letters in files stored
                            expanded_name[i] = toupper( expanded_name[i] );
                        }
                    }
                    else//is encountered when user types cd ..
                    {
                        strncpy(expanded_name, cdArr, strlen(cdArr));
                        expanded_name[11] = '\0';
                    }
                    
                    for(int i = 0; i <= 15; i++)//matching positions of dir[]
                    {
                        char *dirList = (char*)malloc(11); //directory name size 11
                        memset(dirList, ' ', 11);
                        memcpy(dirList, dir[i].DIR_Name, 11);//controlling bytes being compared with expanded_name
                        if( strncmp(expanded_name, dirList, 11) == 0)//match found
                        {
                            store = dir[i].DIR_FirstClusterLow;//April 12th lecture 26:12
                        }
                    }
                    
                    if(strcmp(cdArr, "..") == 0)//user has typed ..
                    {
                        for(int i = 0; i <= 15; i++)
                        {
                            //there are folders past foldera that are marked .., need to know where it's at
                            if(strncmp(dir[i].DIR_Name, "..", 2) == 0)
                            {
                                int offsetMarker = LBAToOffset(dir[i].DIR_FirstClusterLow);//April 12th lecture 26:12
                                FirstSectorofCluster = dir[i].DIR_FirstClusterLow; //becomes our new directory
                                fseek(fp, offsetMarker, SEEK_SET);//move file pointer to offset for the folder
                                fread(&dir[0], 32, 16, fp);
                            }
                        }
                    }
                    else //user must have selected an actual folder to enter
                    {
                        int offsetMarker = LBAToOffset(store);//same logic as above, just using the cluster stored in store
                        FirstSectorofCluster = store;
                        fseek(fp, offsetMarker, SEEK_SET);
                        fread(&dir[0], 32, 16, fp);
                    }
                }
            }
            else
            {
                printf("Error: File system image must be opened first.\n");
            }
        }

        // Code for listing the directory contents
        if(strcmp(token[0], "ls") == 0)
        {
            if(fileIsOpen == 1)
            {
                if(getterPrinter == true || afterClose == 1)
                {
                	int i = 0;
                    for(int j = 0; j <= 65; j++)
                    {
                        int offsetMarker = LBAToOffset(FirstSectorofCluster);//current directory
                        fseek(fp, offsetMarker, SEEK_SET);//move file pointer up to that offset
   
                        for(i = 0; i < 16; i ++)//16 positions in dir
                        {
                            fread(&dir[i], 32, 1, fp);
                            //if DIR_Name[0] == 0xE5, then directory entry is free(no file or directory name in this entry)
                            //0x01 is a read only file, 0x10 is a subdirectory, 0x20 is an archive flag, all should be listed
                            if(dir[i].DIR_Name[0] != (char) 0xe5)//Guess we have to typecast 0xe5 since DIR_Name is a type char
                            {
                                //only want to print files of this type
                                if((dir[i].DIR_Attr == 0x01) || (dir[i].DIR_Attr == 0x10) || (dir[i].DIR_Attr == 0x20))
                                {
                                    char *dirList = (char*)malloc(11); //directory name size 11
                                    memset(dirList, '\0', 11);
                                    memcpy(dirList, dir[i].DIR_Name, 11);
                                    if(j == 64)
                                    {
                                    	printf("%s\n", dirList);
                                    }
                                }
                            }
                        }
                        getterPrinter = false;
                    }
                }
                else
                {
                    int offsetMarker = LBAToOffset(FirstSectorofCluster);//current directory
                    fseek(fp, offsetMarker, SEEK_SET);//move file pointer up to that offset
                    for(int i = 0; i < 16; i ++)//16 positions in dir
                    {
                        fread(&dir[i], 32, 1, fp);
                        //if DIR_Name[0] == 0xE5, then directory entry is free(no file or directory name in this entry)
                        //0x01 is a read only file, 0x10 is a subdirectory, 0x20 is an archive flag, all should be listed
                        if(dir[i].DIR_Name[0] != (char) 0xe5)//Guess we have to typecast 0xe5 since DIR_Name is a type char
                        {
                            //only want to print files of this type
                            if((dir[i].DIR_Attr == 0x01) || (dir[i].DIR_Attr == 0x10) || (dir[i].DIR_Attr == 0x20))
                            {
                                char *dirList = (char*)malloc(11); //directory name size 11
                                memset(dirList, '\0', 11);
                                memcpy(dirList, dir[i].DIR_Name, 11);
                                printf("%s\n", dirList);//have to control the amount of bytes we're printing
                            }
                        }
                    }
                }
            }
            else
            {
                printf("Error: File system image must be opened first.\n");
            }
        }

        // Code for reading the filesystem
        if(strcmp(token[0], "read") == 0) //read <filename> <position> <# bytes>
        {
            int store;
            int fileSize;
            if(fileIsOpen == 1)
            {
                if(token[1] == NULL  || token[2] == NULL || token[3] == NULL)
                {
                    printf("Please give a valid input. \n");
                }
                else//if condition above didn't go through, then we can assume the input was valid
                {
                    char expanded_name[12]; //following example in compare.c
                    char *readArr = token[1]; //workable pointer to where we want to read into
                    memset( expanded_name, ' ', 12);
                    strcpy(readFilename, token[1]);
                    char savedFileName[12];
                    strcpy(savedFileName, token[1]);
                    int position = atoi(token[2]);
                    int endPosition = atoi(token[3]);
                    int matchFound = 0;
                    char *token = strtok (readArr, ".");
                    if(token != NULL)//need this or else we seg fault
                    {
                        strncpy(expanded_name, token, strlen(token)); //store tokenized file into expanded_name
                        token = strtok( NULL, ".");
                        if(token)//compare.c
                        {
                            strncpy( (char *)(expanded_name+8), token, strlen(token));
                        }
             
                        expanded_name[11] = '\0';
                        int i;
             
                        for( i = 0; i < 11; i ++)//code in compare.c, convert user typed token[1] into uppercase, if not already
                        {
                            //set user input to upper to match uppercase letters in files stored
                            expanded_name[i] = toupper( expanded_name[i] );
                        }
                    }
                    else
                    {
                        strncpy(expanded_name, readArr, strlen(readArr));
                        expanded_name[11] = '\0';
                    }
             
                    for(int i = 0; i <= 15; i++)//matching positions of dir[]
                    {
                       char *dirList = (char*)malloc(11); //directory name size 11
                       memset(dirList, ' ', 11);
                       memcpy(dirList, dir[i].DIR_Name, 11);//controlling bytes being compared with expanded_name
                  
                        if( strncmp(expanded_name, dirList, 11) == 0) //match found
                        {
                            store = dir[i].DIR_FirstClusterLow;//April 12th lecture 26:12
                            fileSize = dir[i].DIR_FileSize;
                            matchFound = 1;
                        }
                    }
          
                    if(matchFound == 0)
                    {
                        printf("Error: No matching file to read.\n");
                    }
                    else
                    {
                        fseek(fp, LBAToOffset(store), SEEK_SET);
                        unsigned char *buffer = (unsigned char*)malloc(fileSize);
                        fread(buffer, fileSize, 1, fp);
                        int i;
                   
                        for(i = position; i < position + endPosition; i++)
                        {
                            printf("0x%x ", buffer[i]);
                        }
                        printf("\n");
                    }
                }
            }
            else
            {
                printf("Error: File system image must be opened first.\n");
            }
        }

        // If the command entered into token[0] does not match any of our allowed commands, its a nasty looking if
        if(strcmp(token[0], "open") != 0 && strcmp(token[0], "close") != 0 && strcmp(token[0], "info") != 0 &&
        strcmp(token[0], "stat") != 0 && strcmp(token[0], "get") != 0 && strcmp(token[0], "cd") != 0 &&
        strcmp(token[0], "ls") != 0 && strcmp(token[0], "read") != 0)
        {
            printf("Error: Please enter a valid command.\n");
        }
    
        free( working_root );
    }
    return 0;
}

//Finds the starting address of a block of data given the sector number corresponding to that data block
int LBAToOffset(int32_t sector)
{
    if(sector == 0)//root directory quirk, need to set to 2 or else root directory offset will be lost
    {
        sector = 2;
    }  
    return (( sector - 2 ) * BPB_BytsPerSec) + (BPB_BytsPerSec * BPB_RsvdSecCnt) + (BPB_NumFATs * BPB_FATSz32 * BPB_BytsPerSec);
}

//given a logical block address, look up into the first FAT and return the logical block address of the block in the file.
//If there is no further blocks then return -1
int16_t NextLB( uint32_t sector )
{
    uint32_t FATAddress = ( BPB_BytsPerSec * BPB_RsvdSecCnt ) + ( sector * 4 );
    int16_t val;
    fseek( fp, FATAddress, SEEK_SET );
    fread( &val, 2, 1, fp);
    return val;
}

void decConverter( int decimal )
{
    int temp;
    char hexNumber[100];
    int i = 1;
    while(decimal != 0)
    {
        temp = decimal % 16;
        if( temp < 10)
        {
            temp = temp + 48;
        }
        else
        {
            temp = temp + 55;
        }
        hexNumber[i++] = temp;
        decimal = decimal / 16;
    }

    for(int j = i - 1; j > 0; j --)
    {
        printf("%c", hexNumber[j]);
    }
    printf("\n");
}

