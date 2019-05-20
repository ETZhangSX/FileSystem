//
//  main.cpp
//  FileSystem
//
//  Created by 张顺鑫 on 2019/3/27.
//  Copyright © 2019 张顺鑫. All rights reserved.
//
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/malloc.h>
#include <signal.h>
#include <string>

#define BLOCKSIZE 1024
#define BLOCKNUM  512
#define INODENUM  256
#define FILENAME "file.dat"


typedef struct{
    unsigned short blockSize;
    unsigned short blockNum;
    unsigned short inodeNum;
    unsigned short blockFree;
    unsigned short inodeFree;
}SuperBlock;

typedef struct{
    unsigned short inum;
    char fileName[10];
    unsigned short isDir;  // 0-file 1-dir
    unsigned short iparent;
    unsigned short length;    //if file->filesize  if dir->filenum
    unsigned short blockNum;
}Inode, *PtrInode;

//Fcb用于存储文件与 目录信息，主要用途：将一个目录下的所有文件(包括目录)写入到该目录对应的Block中
typedef struct {
    unsigned short inum;
    char fileName[10];
    unsigned short isDir;
}Fcb, *PtrFcb;


typedef struct{
    char userName[10];
    char passWord[10];
}User;


char blockBitmap[BLOCKNUM];
char inodeBitmap[INODENUM];

SuperBlock superBlock;
User curUser = (User){"root", "root"};
const char *syscmd[] = {
    "help", 
    "ls", 
    "cd", 
    "mkdir", 
    "touch", 
    "cat", 
    "write", 
    "rm", 
    "logout", 
    "exit", 
    "sysinfo", 
    "cls",
    "cp"};
const int cmdNumber = 13;

unsigned short currentDir; //current inodenum

FILE *fp;
const unsigned short superBlockSize = sizeof(superBlock);
const unsigned short blockBitmapSize = sizeof(blockBitmap);
const unsigned short inodeBitmapSize = sizeof(inodeBitmap);
const unsigned short inodeSize = sizeof(Inode);
const unsigned short fcbSize = sizeof(Fcb);
char *argv[5];
int argc;

void createFileSyetem();
void openFileSystem();
void createFile(char *name, int flag);
void list();
int findInodeNum(char *name, int flag);
void cd(char *name);
void cdParent();
void write(char *name);
void read(char *name);
void deleteFile(char *name);
char* copy_help(char* name);
void copy(char *file1, char *file2);
void updateResource();
void pathSet();
void systemInfo();
void help();
int analyse(char *str);
void login();
void stopHandle(int sig);
void commandNotFound(char *cmd);
void command(void);


int main(int argc, char **argv){
    signal(SIGINT, stopHandle);
    login();
    openFileSystem();
    command();
    return 0;
}


void createFileSystem() {
    long len;
    PtrInode fileInode;
    if ((fp = fopen(FILENAME, "wb+")) == NULL) {
        printf("open file %s error...\n", FILENAME);
        exit(1);
    }
    
    //init bitmap
    for(len = 0; len < BLOCKNUM; len++)
        blockBitmap[len] = 0;
    
    for(len = 0; len < INODENUM; len++)
        inodeBitmap[len] = 0;
    
    //memset
    
    for (len = 0; len < (superBlockSize + blockBitmapSize + inodeBitmapSize + inodeSize * INODENUM + BLOCKSIZE * BLOCKNUM); len++) {
        fputc(0, fp);
    }
    rewind(fp);
    
    //init superBlock
    superBlock.blockNum = BLOCKNUM;
    superBlock.blockSize = BLOCKSIZE;
    superBlock.inodeNum = INODENUM;
    superBlock.blockFree = BLOCKNUM - 1;
    superBlock.inodeFree = INODENUM - 1;
    
    fwrite(&superBlock, superBlockSize, 1, fp);
    
    //create root
    fileInode = (PtrInode)malloc(inodeSize);
    fileInode->inum = 0;
    strcpy(fileInode->fileName, "/");
    fileInode->isDir = 1;
    fileInode->iparent = 0;
    fileInode->length = 0;
    fileInode->blockNum = 0;
    
    //write / info to file
    inodeBitmap[0] = 1;
    blockBitmap[0] = 1;
    fseek(fp, superBlockSize, SEEK_SET);
    fwrite(blockBitmap, blockBitmapSize, 1, fp);
    fseek(fp, superBlockSize + blockBitmapSize, SEEK_SET);
    fwrite(inodeBitmap, inodeBitmapSize, 1, fp);
    fseek(fp, superBlockSize + blockBitmapSize + inodeBitmapSize, SEEK_SET);
    fwrite(fileInode, inodeSize, 1, fp);
    fflush(fp);
    
    //point to currentDir
    currentDir = 0;
    
}

/*如果FILENAME可读，则代表之前已有信息，并读取相应数据    如果不可读，则创建文件系统 */
void openFileSystem() {
    
    if((fp = fopen(FILENAME, "rb")) == NULL) {
        createFileSystem();
    }
    else {
        if ((fp = fopen(FILENAME, "rb+")) == NULL){
            printf("open file %s error...\n", FILENAME);
            exit(1);
        }
        rewind(fp);
        //read superBlock from file
        fread(&superBlock, superBlockSize, 1, fp);
        
        //read bitmap from file
        fread(blockBitmap, blockBitmapSize, 1, fp);
        fread(inodeBitmap, inodeBitmapSize, 1, fp);
        
        //init current dir
        currentDir = 0;
    }
}

//flag=0 ->create file   =1 ->directory
void createFile(char *name, int flag) {
    int i, nowBlockNum = 0, nowInodeNUm = 0;
    PtrInode fileInode = (PtrInode)malloc(inodeSize);
    PtrInode parentInode = (PtrInode)malloc(inodeSize);
    PtrFcb fcb = (PtrFcb)malloc(fcbSize);
    
    //the available blockNumber
    for(i = 0; i < BLOCKNUM; i++) {
        if(blockBitmap[i] == 0) {
            nowBlockNum = i;
            break;
        }
    }
    
    //the available inodeNumber
    for(i = 0; i < INODENUM; i++) {
        if(inodeBitmap[i] == 0) {
            nowInodeNUm = i;
            break;
        }
        
    }
    
    //init fileINode struct
    fileInode->blockNum = nowBlockNum;
    strcpy(fileInode->fileName, name);
    fileInode->inum = nowInodeNUm;
    fileInode->iparent = currentDir;
    if(flag == 0) {
        fileInode->isDir = 0;
    }
    else {
        fileInode->isDir = 1;
    }
    fileInode->length = 0;
    
    //write fileInfo to file
    fseek(fp, superBlockSize + blockBitmapSize + inodeBitmapSize + inodeSize * nowInodeNUm, SEEK_SET);
    fwrite(fileInode, inodeSize, 1, fp);
    
    //update superBlock and bitmap
    superBlock.blockFree -= 1;
    superBlock.inodeFree -= 1;
    blockBitmap[nowBlockNum] = 1;
    inodeBitmap[nowInodeNUm] = 1;
    
    //init fcb info
    strcpy(fcb->fileName, fileInode->fileName);
    fcb->inum = fileInode->inum;
    fcb->isDir = fileInode->isDir;
    
    //update to file ...
    
    //update parent dir block info
    fseek(fp, superBlockSize + blockBitmapSize + inodeBitmapSize + currentDir * inodeSize, SEEK_SET);
    fread(parentInode, inodeSize, 1, fp);
    fseek(fp, superBlockSize + blockBitmapSize + inodeBitmapSize + INODENUM * inodeSize + parentInode->blockNum * BLOCKSIZE + parentInode->length * fcbSize, SEEK_SET);
    fwrite(fcb, fcbSize, 1, fp);
    
    //update parent dir inode info
    parentInode->length += 1;
    fseek(fp, superBlockSize + blockBitmapSize + inodeBitmapSize + currentDir * inodeSize, SEEK_SET);
    fwrite(parentInode, inodeSize, 1, fp);
    
    // free resource
    free(fileInode);
    free(parentInode);
    free(fcb);
}


void list() {
    int i;
    PtrFcb fcb = (PtrFcb)malloc(fcbSize);
    PtrInode parentInode = (PtrInode)malloc(inodeSize);
    
    //read parent inode info from file
    fseek(fp, superBlockSize + blockBitmapSize + inodeBitmapSize + currentDir * inodeSize, SEEK_SET);
    fread(parentInode, inodeSize, 1, fp);
    
    //point to parent dir block
    fseek(fp, superBlockSize + blockBitmapSize + inodeBitmapSize + inodeSize * INODENUM + parentInode->blockNum * BLOCKSIZE, SEEK_SET);
    
    //list info
    for(i = 0; i < parentInode->length; i++) {
        fread(fcb, fcbSize, 1, fp);
        printf("Filename: %-10s", fcb->fileName);
        printf("Inode number: %-2d    ", fcb->inum);
        if(fcb->isDir == 1) {
            printf("Directory\n");
        }
        else {
            printf("Regular file\n");
        }
    }
    
    //free resource
    free(fcb);
    free(parentInode);
}

//flag=0 ->find file flag=1 -> find dir
int findInodeNum(char *name, int flag) {
    int i, fileInodeNum = 0;
    PtrInode parentInode = (PtrInode)malloc(inodeSize);
    PtrFcb fcb = (PtrFcb)malloc(fcbSize);
    
    //read current inode info from file
    fseek(fp, superBlockSize + blockBitmapSize + inodeBitmapSize + currentDir * inodeSize, SEEK_SET);
    fread(parentInode, inodeSize, 1, fp);
    
    //read the fcb in the current dir block
    fseek(fp, superBlockSize + blockBitmapSize + inodeBitmapSize + inodeSize * INODENUM + parentInode->blockNum * BLOCKSIZE, SEEK_SET);
    
    for(i = 0; i < parentInode->length; i++) {
        fread(fcb, fcbSize, 1, fp);
        if(flag == 0) {
            if((fcb->isDir == 0)&&(strcmp(name, fcb->fileName) == 0)) {
                fileInodeNum = fcb->inum;
                break;
            }
            
        }
        else {
            if((fcb->isDir == 1)&&(strcmp(name, fcb->fileName) == 0)) {
                fileInodeNum = fcb->inum;
                break;
            }
            
        }
    }
    
    if(i == parentInode->length)
        fileInodeNum = -1;
    
    free(fcb);
    free(parentInode);
    return fileInodeNum;
}

void cd(char *name) {
    int fileInodeNum;
    PtrInode fileInode = (PtrInode)malloc(inodeSize);
    if(strcmp(name, "..") != 0) {
        if (strcmp(name, "/") != 0) {
            fileInodeNum = findInodeNum(name, 1);
            if(fileInodeNum == -1)
                printf("This is no %s directory...\n", name);
            else {
                currentDir = fileInodeNum;
            }
        }
        else {
            fseek(fp, superBlockSize + blockBitmapSize + inodeBitmapSize, SEEK_SET);
            currentDir = 0;
        }
    }
    else {
        fseek(fp, superBlockSize + blockBitmapSize + inodeBitmapSize + currentDir * inodeSize, SEEK_SET);
        fread(fileInode, inodeSize, 1, fp);
        currentDir = fileInode->iparent;
        
    }
    free(fileInode);
}

void cd_l(char *name) {
    int  i;
    char temp[20];
    char *ptr_char;
    char *pathname;
    // argc = 0;
    if (*ptr_char == '/') {
        cd("/");
        ptr_char++;
    }
    for(i = 0, ptr_char = name; *ptr_char != '\0'; ptr_char++) {
        if(*ptr_char != ' ') {
            while(*ptr_char != '/' && (*ptr_char != '\0'))
                temp[i++] = *ptr_char++;
            pathname = (char *)malloc(i + 1);
            strncpy(pathname, temp, i);
            pathname[i] = '\0';
            cd(pathname);
            // argc++;
            i = 0;
            if(*ptr_char == '\0')
                break;
        }
    }
}

void cdParent() {
    PtrInode fileInode = (PtrInode)malloc(inodeSize);
    fseek(fp, superBlockSize + blockBitmapSize + inodeBitmapSize + currentDir * inodeSize, SEEK_SET);
    fread(fileInode, inodeSize, 1, fp);
    currentDir = fileInode->iparent;
    
    free(fileInode);
}

void write(char *name) {
    int fileInodeNum, i = 0;
    char c;
    PtrInode fileInode = (PtrInode)malloc(inodeSize);
    if((fileInodeNum = findInodeNum(name, 1)) != -1) {
        printf("This is a directory, not a file...\n");
        return;
    }
    
    fileInodeNum = findInodeNum(name, 0);
    if(fileInodeNum == -1)
        printf("This is no %s file...\n", name);
    else {
        //get inode->blocknum
        fseek(fp, superBlockSize + blockBitmapSize + inodeBitmapSize + fileInodeNum * inodeSize, SEEK_SET);
        fread(fileInode, inodeSize, 1, fp);
        //point to the block site
        fseek(fp, superBlockSize + blockBitmapSize + inodeBitmapSize + INODENUM * inodeSize + fileInode->blockNum * BLOCKSIZE, SEEK_SET);
        printf("please input file content(stop by #):\n");
        while((c = getchar()) != '#') {
            fputc(c, fp);
            i++;
        }
        
        //update inode->length
        fseek(fp, superBlockSize + blockBitmapSize + inodeBitmapSize + fileInodeNum * inodeSize, SEEK_SET);
        fread(fileInode, inodeSize, 1, fp);
        fileInode->length = i - 1;
        fseek(fp, -inodeSize, SEEK_CUR);
        fwrite(fileInode, inodeSize, 1, fp);

        while (getchar() != '\n');
    }
    
    free(fileInode);
    
}

void read(char *name) {
    int fileInodeNum;
    char c;
    PtrInode fileInode = (PtrInode)malloc(inodeSize);
    if((fileInodeNum = findInodeNum(name, 1)) != -1) {
        printf("This is a directory, not a file...\n");
        return;
    }
    fileInodeNum = findInodeNum(name, 0);
    if(fileInodeNum == -1)
        printf("This is no %s file...\n", name);
    else {
        //get inode->blocknum
        fseek(fp, superBlockSize + blockBitmapSize + inodeBitmapSize + fileInodeNum * inodeSize, SEEK_SET);
        fread(fileInode, inodeSize, 1, fp);
        //point to the block site
        fseek(fp, superBlockSize + blockBitmapSize + inodeBitmapSize + INODENUM * inodeSize + fileInode->blockNum * BLOCKSIZE, SEEK_SET);
        
        //read content
        if(fileInode->length != 0) {
            while((c = fgetc(fp)) != EOF) {
                putchar(c);
            }
            printf("\n");
        }
    }
    free(fileInode);
}

/*delete 一个文件，需要修改SuperBlock,Blockbitmap,Inodebitmap,并更新父节点长度，删除父节点Block中存储的该文件fcb*/
void deleteFile(char *name) {
    int fileInodeNum, i;
    PtrInode fileInode = (PtrInode)malloc(inodeSize);
    PtrInode parentInode = (PtrInode)malloc(inodeSize);
    Fcb fcb[20];
    if(((fileInodeNum = findInodeNum(name, 0)) == -1)&&((fileInodeNum = findInodeNum(name, 1)) == -1)) {
        printf("This is no %s...\n", name);
    }
    else {
        if((fileInodeNum = findInodeNum(name, 0)) == -1) {
            fileInodeNum = findInodeNum(name, 1);
        }
        
        fseek(fp, superBlockSize + blockBitmapSize + inodeBitmapSize + fileInodeNum * inodeSize, SEEK_SET);
        fread(fileInode, inodeSize, 1, fp);
        
        fseek(fp, superBlockSize + blockBitmapSize + inodeBitmapSize + fileInode->iparent * inodeSize, SEEK_SET);
        fread(parentInode, inodeSize, 1, fp);
        
        
        //update parent info
        fseek(fp, superBlockSize + blockBitmapSize + inodeBitmapSize + INODENUM * inodeSize + parentInode->blockNum * BLOCKSIZE, SEEK_SET);
        for(i = 0; i < parentInode->length; i++) {
            fread(&fcb[i], fcbSize, 1, fp);
            //fcb[i]=tmp;
        }
        
        fseek(fp, superBlockSize + blockBitmapSize + inodeBitmapSize + INODENUM * inodeSize + parentInode->blockNum * BLOCKSIZE, SEEK_SET);
        for(i = 0; i < BLOCKSIZE; i++)
            fputc(0, fp);
        
        fseek(fp, superBlockSize + blockBitmapSize + inodeBitmapSize + INODENUM * inodeSize + parentInode->blockNum * BLOCKSIZE, SEEK_SET);
        for(i = 0; i < parentInode->length; i++) {
            if((strcmp(fcb[i].fileName, name)) != 0) {
                fwrite(&fcb[i], fcbSize, 1, fp);
            }
        }
        
        parentInode->length -= 1;
        fseek(fp, superBlockSize + blockBitmapSize + inodeBitmapSize + fileInode->iparent * inodeSize, SEEK_SET);
        fwrite(parentInode, inodeSize, 1, fp);
        //update bitmap
        inodeBitmap[fileInodeNum] = 0;
        blockBitmap[fileInode->blockNum] = 0;
        
        //update superblock
        superBlock.blockFree += 1;
        superBlock.inodeFree += 1;
    }
    
    free(fileInode);
    free(parentInode);
    
}

char* copy_help(char* name) {
    char *ptr_char = name;

    while (*ptr_char != '\0') ptr_char++;
    while (*ptr_char != '/') ptr_char--;

    *ptr_char = '\0';
    ptr_char++;

    cd_l(name);
    return ptr_char;
}

void copy(char *file1, char *file2) {
    unsigned short currentDir_cp = currentDir;
    FILE* rfp;
    if ((rfp = fopen(FILENAME, "rb+")) == NULL) {
        printf("open file %s error...\n", FILENAME);
        exit(1);
    }
    char *file1_name = copy_help(file1);
    int fileInodeNum;
    char c;

    PtrInode fileInode_r = (PtrInode)malloc(inodeSize);
    if((fileInodeNum = findInodeNum(file1_name, 1)) != -1) {
        printf("This is a directory, not a file...\n");
        return;
    }

    fileInodeNum = findInodeNum(file1_name, 0);
    if(fileInodeNum == -1)
        printf("This is no %s file...\n", file1_name);
    else {
        //get inode->blocknum
        fseek(rfp, superBlockSize + blockBitmapSize + inodeBitmapSize + fileInodeNum * inodeSize, SEEK_SET);
        fread(fileInode_r, inodeSize, 1, rfp);
        //point to the block site
        fseek(rfp, superBlockSize + blockBitmapSize + inodeBitmapSize + INODENUM * inodeSize + fileInode_r->blockNum * BLOCKSIZE, SEEK_SET);
        
        currentDir = currentDir_cp;
        fseek(fp, superBlockSize + blockBitmapSize + inodeBitmapSize + currentDir * inodeSize, SEEK_SET);

        char *file2_name = copy_help(file2);
        createFile(file2_name, 0);
        int fileInodeNum, i = 0;
        char c;
        PtrInode fileInode = (PtrInode)malloc(inodeSize);
        
        if((fileInodeNum = findInodeNum(file2_name, 1)) != -1) {
            printf("This is a directory, not a file...\n");
            return;
        }
    
        fileInodeNum = findInodeNum(file2_name, 0);
        if(fileInodeNum == -1)
            printf("This is no %s file...\n", file2);
        else {
            //get inode->blocknum
            fseek(fp, superBlockSize + blockBitmapSize + inodeBitmapSize + fileInodeNum * inodeSize, SEEK_SET);
            fread(fileInode, inodeSize, 1, fp);
            //point to the block site
            fseek(fp, superBlockSize + blockBitmapSize + inodeBitmapSize + INODENUM * inodeSize + fileInode->blockNum * BLOCKSIZE, SEEK_SET);
            
            if(fileInode_r->length != 0) {
                while((c = fgetc(rfp)) != EOF) {
                    fputc(c, fp);
                    i++;
                }
            }
            //update inode->length
            fseek(fp, superBlockSize + blockBitmapSize + inodeBitmapSize + fileInodeNum * inodeSize, SEEK_SET);
            fread(fileInode, inodeSize, 1, fp);
            fileInode->length = i - 1;
            fseek(fp, -inodeSize, SEEK_CUR);
            fwrite(fileInode, inodeSize, 1, fp);
    
        }
        free(fileInode);
    }
    free(fileInode_r);
    fclose(rfp);

    currentDir = currentDir_cp;
    fseek(fp, superBlockSize + blockBitmapSize + inodeBitmapSize + currentDir * inodeSize, SEEK_SET);
}

void updateResource() {
    rewind(fp);
    fwrite(&superBlock, superBlockSize, 1, fp);
    fwrite(blockBitmap, blockBitmapSize, 1, fp);
    fwrite(inodeBitmap, inodeBitmapSize, 1, fp);
    fclose(fp);
}

void pathSet() {
    PtrInode curInode = (PtrInode)malloc(inodeSize);
    fseek(fp, superBlockSize + blockBitmapSize + inodeBitmapSize + currentDir * inodeSize, SEEK_SET);
    fread(curInode, inodeSize, 1, fp);
    printf("\033[1;40;32mMacBook-Pro\033[0m:\033[1;40;34m%s %s\033[0m$ ", curInode->fileName, curUser.userName);
    free(curInode);
}

void systemInfo() {
    printf("Sum of block number: %d\n", superBlock.blockNum);
    printf("Each block size: %d\n", superBlock.blockSize);
    printf("Free of block number: %d\n", superBlock.blockFree);
    printf("Sum of inode number: %d\n", superBlock.inodeNum);
    printf("Free of inode number: %d\n", superBlock.inodeFree);
}

void help() {
    printf("command: \n\
           help\t---  show help menu \n\
           sysinfo\t---  show system base information \n\
           cls\t---  clear the screen \n\
           cd\t---  change directory \n\
           mkdir\t---  make directory   \n\
           touch\t---  create a new file \n\
           cat\t---  read a file \n\
           write\t---  write something to a file \n\
           logout\t---  exit user \n\
           rm\t---  delete a directory or a file \n\
           exit\t---  exit this system\n");
}

int analyse(char *str) {
    int  i;
    char temp[20];
    char *ptr_char;
    argc = 0;
    // printf("seg1\n");
    for(i = 0, ptr_char = str; *ptr_char != '\0'; ptr_char++) {
        if(*ptr_char != ' ') {
            while(*ptr_char != ' ' && (*ptr_char != '\0'))
                temp[i++] = *ptr_char++;
            argv[argc] = (char *)malloc(i + 1);
            strncpy(argv[argc], temp, i);
            argv[argc][i] = '\0';
            argc++;
            i = 0;
            if(*ptr_char == '\0')
                break;
        }
    }
    // printf("seg2\n");
    if(argc != 0) {
        for(i = 0; (i < cmdNumber) && strcmp(argv[0], syscmd[i]); i++);
        return i;
    }
    else
        return cmdNumber;
    return 0;
}

void login() {
    char userName[10];
    char passWord[10];
    while(1) {
        printf("login:");
        gets(userName);
        system("stty -echo");
        printf("passwd:");
        gets(passWord);
        system("stty echo");
        printf("\n");
        if(strcmp(userName, curUser.userName) == 0 && strcmp(passWord, curUser.passWord) == 0)
            break;
    }
}

void stopHandle(int sig) {
    printf("\nPlease wait..., update resource\n");
    updateResource();
    exit(0);
}

void commandNotFound(char *cmd) {
    printf("%s: command not found\n", argv[0]);
}

void command(void) {
    char cmd[20];
    do
    {
        pathSet();
        gets(cmd);
        // printf("gets cmd\n");
        switch(analyse(cmd))
        {
            case 0:
                help();
                break;
            case 1:
                list();
                break;
            case 2:
                if (argv[1] == NULL) {
                    printf("usage: cd directory ...\n");
                }
                else
                    // cd(argv[1]);
                    cd_l(argv[1]);
                break;
            case 3:
                if (argv[1] == NULL) {
                    printf("usage: mkdir directory ...\n");
                }
                else
                    createFile(argv[1], 1);
                break;
            case 4:
                if (argv[1] == NULL) {
                    printf("usage: touch file ...\n");
                }
                else
                    createFile(argv[1], 0);
                break;
            case 5:
                if (argv[1] == NULL) {
                    printf("usage: cat file\n");
                }
                else
                    read(argv[1]);
                break;
            case 6:
                if (argv[1] == NULL) {
                    printf("usage: write file\n");
                }
                else
                    write(argv[1]);
                break;
            case 7:
                if (argv[1] == NULL) {
                    printf("usage: rm file\n");
                }
                else
                    deleteFile(argv[1]);
                break;
            case 8:
                updateResource();
                login();
                openFileSystem();
                command();
                break;
            case 9:
                updateResource();
                exit(0);
                break;
            case 10:
                systemInfo();
                break;
            case 11:
                system("clear");
                break;
            case 12:
                if (argv[2] == NULL) {
                    printf("usage: cp file1 file2\n");
                }
                else
                    copy(argv[1], argv[2]);
                break;
            default:
                commandNotFound(argv[0]);
                break;
        }
        for (int i = 0; i < 5; i++) {
            argv[i] = NULL;
        }
    }while(1);
}
