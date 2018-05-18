#include <stdio.h>
#include <stdlib.h>
#include "disk.h"
#include "fs.h"
#include <string.h>

FileDescTable* pFileDescTable = NULL;

int getToken(const char* szName, char namearr[10][12]){
    int j=0;
    char bfpath[15];

    strcpy(bfpath, szName);
    strcat(bfpath, "\0");
    char *token;
    token = strtok(bfpath, "/");
    do{
        strcpy(namearr[j], token);
        j++;
    }while(token = strtok(NULL, "/"));
    return j;
}

int GetLastInodeDirBlock(char *name[10], Inode* inode, DirEntry* direct, int *parent_inodeno, int *blkno, int path_num, int* indirect){ 
    //	indirect = NULL ; //parent, blkno는 &으로 함수 콜
    int i, k, w;
    char flag = 0;

    /*root*/
    GetInode(0, inode);
    if(path_num == 0) DevReadBlock(*blkno, (char*)direct);
    for(k=0; k<path_num;k++){
        flag = 0;
        if(inode->dirBlockPtr[0] && flag == 0){
            *blkno = inode->dirBlockPtr[0];
            DevReadBlock(*blkno, (char*)direct);
            for(i=0;i<4;i++)
            {
                if(!strcmp(direct[i].name, name[k]))
                {
                    *parent_inodeno = direct[i].inodeNum;
                    memset(inode, 0, sizeof(Inode));
                    GetInode(*parent_inodeno, inode);
                    memset(direct, 0, BLOCK_SIZE);
                    flag = 1;
                    break;
                }
            }
        }
        if(inode->dirBlockPtr[1] && flag == 0)
        {
            *blkno = inode->dirBlockPtr[1];
            DevReadBlock(*blkno, (char*)direct);
            for(i=0;i<4;i++)
            {
                if(!strcmp(direct[i].name, name[k]))
                {
                    *parent_inodeno = direct[i].inodeNum;
                    memset(inode, 0, sizeof(Inode));
                    GetInode(*parent_inodeno, inode);
                    memset(direct, 0, BLOCK_SIZE);
                    flag = 1;
                    break;
                }
            }
        }
        if(inode->indirBlockPtr && flag == 0){
            indirect = (int*)calloc(16, 4);
            DevReadBlock(inode->indirBlockPtr, (char*)indirect);
            for(w=0;w<16;w++){
                memset(direct, 0, BLOCK_SIZE);
                *blkno = indirect[w];
                DevReadBlock(*blkno, (char*)direct);
                for(i=0;i<4;i++)
                    if(!strcmp(direct[i].name, name[k]))
                    {
                        *parent_inodeno = direct[i].inodeNum;
                        memset(inode, 0, sizeof(Inode));
                        GetInode(*parent_inodeno, inode);
                        memset(direct, 0, BLOCK_SIZE);
                        flag = 1;
                        break;
                    }
            }
        }
    }
    return 0;
}//return fail : -1, success : 0

int		OpenFile(const char* szFileName, OpenFlag flag)
{
    int inodeno, blkno;
    int i,j, w, path_num, k;
    int parent_inodeno;
    char dirname[10][12];
    memset(dirname, 0, 10);
    path_num = getToken(szFileName, &dirname);

    if(flag == OPEN_FLAG_CREATE) j = path_num;
    else j = path_num+1;
    /*find Dir block, inode*/
    Inode* inode = (Inode*)calloc(1,sizeof(Inode));
    DirEntry* direct = (DirEntry*)calloc(4,sizeof(DirEntry));
    int* indirect = NULL;
    char loop_flag = 0;

    GetInode(0, inode);
    if(path_num == 1) {
        blkno = inode->dirBlockPtr[0];
        DevReadBlock(blkno, (char*)direct);
    }
    for(k=0; k<j-1;k++){
        loop_flag = 0;
        //      parent_inodeno = my_inodeno; 
        if(inode->dirBlockPtr[0] && loop_flag == 0){
            blkno = inode->dirBlockPtr[0];
            DevReadBlock(blkno, (char*)direct);
            for(i=0;i<4;i++)
            {
                if(!strcmp(direct[i].name, dirname[k]))
                {
                    parent_inodeno = direct[i].inodeNum;
                    memset(inode, 0, sizeof(Inode));
                    GetInode(parent_inodeno, inode);
                    memset(direct, 0, BLOCK_SIZE);
                    loop_flag = 1;
                    break;
                }
            }
        }
        if(inode->dirBlockPtr[1] && loop_flag == 0)
        {
            blkno = inode->dirBlockPtr[1];
            memset(direct, 0, BLOCK_SIZE);
            DevReadBlock(blkno, (char*)direct);
            for(i=0;i<4;i++)
            {
                if(!strcmp(direct[i].name, dirname[k]))
                {
                    parent_inodeno = direct[i].inodeNum;
                    memset(inode, 0, sizeof(Inode));
                    GetInode(parent_inodeno, inode);
                    loop_flag = 1;
                    break;
                }
            }
        }
        if(inode->indirBlockPtr && loop_flag == 0){
            indirect = (int*)calloc(16, 4);
            DevReadBlock(inode->indirBlockPtr, (char*)indirect);
            for(w=0;w<16;w++){
                memset(direct, 0, BLOCK_SIZE);
                blkno = indirect[w];
                DevReadBlock(blkno, (char*)direct);
                for(i=0;i<4;i++)
                    if(!strcmp(direct[i].name, dirname[k]))
                    {
                        parent_inodeno = direct[i].inodeNum;
                        memset(inode, 0, sizeof(Inode));
                        GetInode(parent_inodeno, inode);
                        memset(direct, 0, BLOCK_SIZE);
                        loop_flag = 1;
                        break;
                    }
                if(loop_flag == 1) break;
            }
        }
    }
    if(flag == OPEN_FLAG_READWRITE)
        inodeno = parent_inodeno;

    if(flag == OPEN_FLAG_CREATE) //create file
    {
        /*set dir entry of new file*/
        DevReadBlock(inode->dirBlockPtr[0], (char*)direct);
        inodeno = GetFreeInodeNum();
        SetInodeBitmap(inodeno);
        inode->size +=BLOCK_SIZE;
        for(i=0;i<4;i++)
        {
  //          printf("check  %s\n", direct[i].name);
            if(direct[i].name[0] == 0)
            {
                strcpy(direct[i].name, dirname[path_num-1]);
                direct[i].inodeNum = inodeno;
                DevWriteBlock(inode->dirBlockPtr[0], direct);
                break;
            }
        }
        if(i>=4){
            memset(direct, 0, BLOCK_SIZE);
            if(inode->dirBlockPtr[1] == 0){
                int bf = GetFreeBlockNum();
                SetBlockBitmap(bf);
      //          printf("dirBlock 1 num : %d\n", bf);
                strcpy(direct[0].name, dirname[path_num-1]);
                direct[0].inodeNum = inodeno;
                DevWriteBlock(bf, direct); //new dir entry block
                inode->dirBlockPtr[1] = bf;
            }
            else 
            {
                DevReadBlock(inode->dirBlockPtr[1], direct);
                for(i=0;i<4;i++)
                {
     //               printf("check 2 %s\n", direct[i].name);
                    if(direct[i].name[0] == 0){
                        strcpy(direct[i].name, dirname[path_num-1]);
                        direct[i].inodeNum = inodeno;
                        DevWriteBlock(inode->dirBlockPtr[1], direct);
                        break;
                    }
                }   
                if(i>=4){
                    if(inode->indirBlockPtr == 0){
                        if(indirect != NULL)free(indirect);
                        indirect = (int*)calloc(16, 4);
                        memset(direct, 0, BLOCK_SIZE);
                        int bf = GetFreeBlockNum(); //indirect
                        SetBlockBitmap(bf);
         //               printf("indirect num : %d\n", bf);
                        inode->indirBlockPtr= bf;
                        bf = GetFreeBlockNum(); //next dir entry block
                        SetBlockBitmap(bf);
         //               printf("indirect index block num : %d\n", bf);
                        indirect[0] = bf;
                        DevWriteBlock(inode->indirBlockPtr, indirect);
                        strcpy(direct[0].name, dirname[path_num-1]);
                        direct[0].inodeNum = inodeno;
                        DevWriteBlock(bf, direct);
                    }
                    else{
                        memset(direct, 0, BLOCK_SIZE);
                        if(indirect != NULL) free(indirect);
                        indirect = (int*)calloc(16,4);
                        DevReadBlock(inode->indirBlockPtr, indirect);
                        for(k=0;k<16;k++)
                        {
                            if(indirect[k] == 0) break;
                            memset(direct, 0, BLOCK_SIZE);
                            DevReadBlock(indirect[k], direct);
                            for(i=0;i<4;i++)
                            {
                               // printf("check %d %s\n",k+3, direct[i].name);
                                if(direct[i].name[0] == 0){
                                    strcpy(direct[i].name, dirname[path_num-1]);
                                    direct[i].inodeNum = inodeno;
                                    DevWriteBlock(indirect[k], direct);
                                    k=16; break;
                                }
                            }
                        }
                        if(k<16)
                        {
                            int bf = GetFreeBlockNum();
                            SetBlockBitmap(bf);
                            //printf("indirect index blk num : %d, k:%d\n", bf,k);
                            indirect[k] = bf;
                            DevWriteBlock(inode->indirBlockPtr, indirect);

                            memset(direct, 0, BLOCK_SIZE);
                            strcpy(direct[0].name, dirname[path_num-1]);
                            direct[0].inodeNum = inodeno;
                            DevWriteBlock(bf, direct);
                        }
                    }
                }
            }
        }

        PutInode(parent_inodeno, inode);
        /*add file inode*/
        GetInode(inodeno, inode);
        inode->size = 0;
        inode->type = FILE_TYPE_FILE; //enum
        PutInode(inodeno, inode);
        (pFileSysInfo->numAllocInodes)++;
        DevWriteBlock(0, pFileSysInfo);
    } 	
    /*file descriptor*/
    char check = 1; //true
    int fd = -1;
    if(pFileDescTable == NULL) pFileDescTable = (FileDescTable*)calloc(1,sizeof(FileDescTable));
    for(i=0;i<MAX_FD_ENTRY_LEN;i++) //파일존재
    {
        if(pFileDescTable->file[i].bUsed == 0) //false
        {
            memset(inode, 0, sizeof(Inode));
            GetInode(pFileDescTable->file[i].inodeNum, inode);
            fd = i;
            break;
        }
    }
    if(fd != -1){
        pFileDescTable->file[i].inodeNum = inodeno;
        pFileDescTable->file[i].fileOffset = 0;
        pFileDescTable->file[i].bUsed = 1; //true
    }

    free(inode);
    free(direct);
    return fd;
}


int		WriteFile(int fileDesc, char* pBuffer, int length)
{
    Inode* inode = (Inode*)calloc(1,sizeof(Inode));
    int inodeno = pFileDescTable->file[fileDesc].inodeNum;
    GetInode(inodeno, inode);
    int blkno,i,result,w;
    char *fileBlk = (char*)malloc(BLOCK_SIZE);
    int *indirect = NULL;
    int offset = pFileDescTable->file[fileDesc].fileOffset;
    int blkno2; 
    for(i=0; i<length/BLOCK_SIZE; i++){
        blkno = GetFreeBlockNum();
        SetBlockBitmap(blkno);
        if(inode->dirBlockPtr[0] == 0)
            inode->dirBlockPtr[0] = blkno;
        else if(inode->dirBlockPtr[1] == 0)
            inode->dirBlockPtr[1] = blkno;
        else if(inode->indirBlockPtr == 0)
        { 
            inode->indirBlockPtr = blkno;
            indirect = (int*)calloc(16, 4);
            blkno2 = GetFreeBlockNum();
            SetBlockBitmap(blkno2);
            indirect[0] = blkno2;
            DevWriteBlock(blkno, indirect);
            (pFileSysInfo->numAllocBlocks)++;
            (pFileSysInfo->numFreeBlocks)--;
            blkno = blkno2;
        }
        else
        {
            if(indirect == NULL) indirect = (int*)calloc(16, 4);
            DevReadBlock(inode->indirBlockPtr, indirect);
            for(w=1;w<16;w++)
                if(indirect[w] == 0) {
                    indirect[w] = blkno;
                    break;}

            DevWriteBlock(inode->indirBlockPtr, indirect);
        }

        inode->size +=BLOCK_SIZE;
        PutInode(inodeno, inode);
        (pFileSysInfo->numAllocBlocks)++;
        (pFileSysInfo->numFreeBlocks)--;
        memset(fileBlk, 0, BLOCK_SIZE);
        strcpy(fileBlk, &pBuffer[BLOCK_SIZE*i]);
        DevWriteBlock(blkno, fileBlk);
    }
    DevWriteBlock(0, pFileSysInfo);
    pFileDescTable->file[fileDesc].fileOffset = offset+length;

    return length;
}

int		ReadFile(int fileDesc, char* pBuffer, int length)
{
    char *pBuf = (char*)malloc(BLOCK_SIZE);
    Inode* inode = (Inode*)malloc(sizeof(Inode));
    int* indirect = NULL;
    int i;
    int cursor = pFileDescTable->file[fileDesc].fileOffset;
    int offset = cursor;
    GetInode(pFileDescTable->file[fileDesc].inodeNum, inode);

    for(i=0; i<length/BLOCK_SIZE; i++)
    {
        if(cursor < BLOCK_SIZE*2)
            DevReadBlock(inode->dirBlockPtr[cursor/BLOCK_SIZE], pBuf);
        else
        {
            if(indirect == NULL) {indirect = (int*)calloc(16,4); DevReadBlock(inode->indirBlockPtr, indirect);}
            DevReadBlock(indirect[cursor/BLOCK_SIZE - 2], pBuf);
        }
        cursor += BLOCK_SIZE;
        strcpy(&pBuffer[i*BLOCK_SIZE], pBuf);
    }
    pFileDescTable->file[fileDesc].fileOffset = offset + length;

    free(pBuf);
    free(inode);
    if(indirect != NULL) free(indirect);
    return length;
}


int		CloseFile(int fileDesc)
{
    pFileDescTable->file[fileDesc].bUsed = 0; 
    pFileDescTable->file[fileDesc].fileOffset = 0;
    pFileDescTable->file[fileDesc].inodeNum = 0;
}

int		RemoveFile(const char* szFileName)
{
    int i,k, w, path_num;
    int blkno, blknum=0;
    int my_inodeno=0, indexno=0, parent_inodeno;
    char dirname[10][12];
    memset(dirname, 0, 10);
    path_num = getToken(szFileName, &dirname);

    /*find Dir block, inode*/
    Inode* inode = (Inode*)calloc(1,sizeof(Inode));
    DirEntry* direct = (DirEntry*)calloc(4,sizeof(DirEntry));
    int* indirect = NULL;
    char flag = 0;

   for(k=0; k<path_num;k++){
        flag = 0;
        parent_inodeno = my_inodeno;
        GetInode(my_inodeno, inode); 
        if(inode->dirBlockPtr[0] && flag == 0){
            DevReadBlock(inode->dirBlockPtr[0], (char*)direct);
            for(i=0;i<4;i++)
            {
                if(!strcmp(direct[i].name, dirname[k])) //remove dir
                {
                    my_inodeno = direct[i].inodeNum;
                    blkno = inode->dirBlockPtr[0];
                    indexno = i;
                    flag = 1;
                    break;
                }
            }
        }
        if(inode->dirBlockPtr[1] && flag == 0)
        {
            memset(direct, 0, BLOCK_SIZE);
            DevReadBlock(inode->dirBlockPtr[1], (char*)direct);
            for(i=0;i<4;i++)
            {
                if(!strcmp(direct[i].name, dirname[k]))
                {
                    my_inodeno = direct[i].inodeNum;
                    blkno = inode->dirBlockPtr[1];
                    indexno = i;
                    flag = 1;
                    break;
                }
            }
        }
        if(inode->indirBlockPtr && flag == 0){
            indirect = (int*)calloc(16, 4);
            DevReadBlock(inode->indirBlockPtr, (char*)indirect);
            for(w=0;w<16;w++){
                memset(direct, 0, BLOCK_SIZE);
                DevReadBlock(indirect[w], (char*)direct);
                for(i=0;i<4;i++)
                    if(!strcmp(direct[i].name, dirname[k]))
                    {
                        my_inodeno = direct[i].inodeNum;
                        blkno = indirect[w];
                        indexno = i;
                        flag = 1;
                        break;
                    }
                if(flag==1)break;
            }
        }
    }
    if(flag == 0) return -1; //don't find file

    /*reset dir entry*/
/*    if(indexno == 0 && strcmp(direct[0].name, ".") && direct[1].name[0] == 0) //"."가 아니고 인덱스 번호가 0번일때
    {
        ResetBlockBitmap(blkno);
        if(indirect != NULL){
            if(w==0){
                memset(indirect, 0, BLOCK_SIZE);
                DevWriteBlock(inode->indirBlockPtr, indirect);
                ResetBlockBitmap(inode->indirBlockPtr);
                inode->indirBlockPtr = 0;
            }
            else{
                indirect[w] = 0;
                DevWriteBlock(inode->indirBlockPtr, indirect);}
        }
        else if(inode->dirBlockPtr[1] == parent_inodeno)
        {
            inode->dirBlockPtr[1]=0;
        }
    } */
    inode->size -= BLOCK_SIZE;
    strcpy(direct[indexno].name, "");
    direct[indexno].inodeNum = 0;
    DevWriteBlock(blkno, direct);
    PutInode(parent_inodeno, inode);

    /*remove file block*/
    GetInode(my_inodeno, inode);
    char* buf = (char*)calloc(BLOCK_SIZE,1);
    if(inode->dirBlockPtr[0] != 0)
    {
        DevWriteBlock(inode->dirBlockPtr[0],buf);
        ResetBlockBitmap(inode->dirBlockPtr[0]);
        inode->dirBlockPtr[0]=0;
        blknum++;
    }
    if(inode->dirBlockPtr[1] != 0)
    {
        DevWriteBlock(inode->dirBlockPtr[1],buf);
        ResetBlockBitmap(inode->dirBlockPtr[1]);
        inode->dirBlockPtr[1]=0;
        blknum++;
    }
    if(inode->indirBlockPtr != 0)
    {
        if(indirect==NULL) indirect = (int*)calloc(16,4);
        DevReadBlock(inode->indirBlockPtr, indirect);
        for(i=0; i<16; i++)
            if(indirect[i] != 0)
            {
                DevWriteBlock(indirect[i],buf);
                ResetBlockBitmap(indirect[i]);
                indirect[i]=0;
                blknum++;
            }
        ResetBlockBitmap(inode->indirBlockPtr);
        DevWriteBlock(inode->indirBlockPtr, indirect);
        inode->indirBlockPtr = 0;
        blknum++;
    }

    /*remove file inode*/
    ResetInodeBitmap(my_inodeno);
    //    printf("reset inodeno :%d\n", my_inodeno);    	
    memset(inode, 0, sizeof(Inode));
    PutInode(my_inodeno, inode);
    (pFileSysInfo->numAllocBlocks) -= blknum;
    (pFileSysInfo->numFreeBlocks) += blknum;
    (pFileSysInfo->numAllocInodes)--;
    DevWriteBlock(0, pFileSysInfo);

    free(direct); free(inode);
    if(indirect != NULL) free(indirect);
    return 0;
}


int		MakeDir(const char* szDirName)
{
    int i,k, w, path_num;
    int inodeno, blkno;
    int parent_inodeno=0, ptr=0;
    char dirname[10][12]; //set max path dir num
    memset(dirname, 0, 10);

    path_num = getToken(szDirName, &dirname);

    /*Get Last Inode, Directory Block(direct/indirect)*/
    Inode* inode = (Inode*)calloc(1,sizeof(Inode));
    DirEntry* direct = (DirEntry*)calloc(4,sizeof(DirEntry));
    int* indirect = NULL;
    char flag = 0;

    GetInode(0, inode);
    if(path_num == 1) {
        blkno = inode->dirBlockPtr[0];
        DevReadBlock(blkno, (char*)direct);
    }
    for(k=0; k<path_num-1;k++){
        flag = 0;
        if(inode->dirBlockPtr[0] && flag == 0){
            blkno = inode->dirBlockPtr[0];
            DevReadBlock(blkno, (char*)direct);
            for(i=0;i<4;i++)
            {
                if(!strcmp(direct[i].name, dirname[k]))
                {
                    parent_inodeno = direct[i].inodeNum;
                    memset(inode, 0, sizeof(Inode));
                    GetInode(parent_inodeno, inode);
                    memset(direct, 0, BLOCK_SIZE);
                    flag = 1;
                    break;
                }
            }
        }
        if(inode->dirBlockPtr[1] && flag == 0)
        {
            blkno = inode->dirBlockPtr[1];
            memset(direct, 0, BLOCK_SIZE);
            DevReadBlock(blkno, (char*)direct);
            for(i=0;i<4;i++)
            {
                if(!strcmp(direct[i].name, dirname[k]))
                {
                    parent_inodeno = direct[i].inodeNum;
                    memset(inode, 0, sizeof(Inode));
                    GetInode(parent_inodeno, inode);
                    flag = 1;
                    break;
                }
            }
        }
        if(inode->indirBlockPtr && flag == 0){
            indirect = (int*)calloc(16, 4);
            DevReadBlock(inode->indirBlockPtr, (char*)indirect);
            for(w=0;w<16;w++){
                memset(direct, 0, BLOCK_SIZE);
                blkno = indirect[w];
                DevReadBlock(blkno, (char*)direct);
                for(i=0;i<4;i++)
                    if(!strcmp(direct[i].name, dirname[k]))
                    {
                        parent_inodeno = direct[i].inodeNum;
                        memset(inode, 0, sizeof(Inode));
                        GetInode(parent_inodeno, inode);
                        memset(direct, 0, BLOCK_SIZE);
                        flag = 1;
                        break;
                    }
                if(flag == 1) break;
            }
        }
    }

    DevReadBlock(inode->dirBlockPtr[0], (char*)direct);
    inodeno = GetFreeInodeNum();
    for(i=0;i<4;i++)
    {
        if(!strcmp(direct[i].name, dirname[path_num-1]))
        {
            free(inode); free(direct);
            return -1;}
        if(direct[i].name[0] == 0)
        {
            strcpy(direct[i].name, dirname[path_num-1]);
            direct[i].inodeNum = inodeno;
            DevWriteBlock(inode->dirBlockPtr[0], direct);
            break;
        }
    }
    if(i>=4){
        memset(direct, 0, BLOCK_SIZE);
        if(inode->dirBlockPtr[1] == 0){
            int bf = GetFreeBlockNum();
            SetBlockBitmap(bf);
            //    printf("dirBlock 1 num : %d\n", bf);
            strcpy(direct[0].name, dirname[path_num-1]);
            direct[0].inodeNum = inodeno;
            DevWriteBlock(bf, direct); //new dir entry block
            inode->dirBlockPtr[1] = bf;
            PutInode(parent_inodeno, inode);
        }
        else 
        {
            DevReadBlock(inode->dirBlockPtr[1], direct);
            for(i=0;i<4;i++)
            {
                if(!strcmp(direct[i].name, dirname[path_num-1]))
                {
                    free(inode); free(direct);
                    return -1;}
                //         printf("check 2 %s\n", direct[i].name);
                if(direct[i].name[0] == 0){
                    strcpy(direct[i].name, dirname[path_num-1]);
                    direct[i].inodeNum = inodeno;
                    DevWriteBlock(inode->dirBlockPtr[1], direct);
                    break;
                }
            }   
            if(i>=4){
                if(inode->indirBlockPtr == 0){
                    if(indirect != NULL)free(indirect);
                    indirect = (int*)calloc(16, 4);
                    memset(direct, 0, BLOCK_SIZE);
                    int bf = GetFreeBlockNum(); //indirect
                    SetBlockBitmap(bf);
                    //          printf("indirect num : %d\n", bf);
                    inode->indirBlockPtr= bf;
                    bf = GetFreeBlockNum(); //next dir entry block
                    SetBlockBitmap(bf);
                    //          printf("indirect index block num : %d\n", bf);
                    indirect[0] = bf;
                    DevWriteBlock(inode->indirBlockPtr, indirect);
                    strcpy(direct[0].name, dirname[path_num-1]);
                    direct[0].inodeNum = inodeno;
                    DevWriteBlock(bf, direct);
                    PutInode(parent_inodeno, inode);
                    //              printf("debug: plus indirect table\n");
                }
                else{
                    memset(direct, 0, BLOCK_SIZE);
                    if(indirect != NULL) free(indirect);
                    indirect = (int*)calloc(16,4);
                    DevReadBlock(inode->indirBlockPtr, indirect);
                    for(k=0;k<16;k++)
                    {
                        if(indirect[k] == 0) break;
                        memset(direct, 0, BLOCK_SIZE);
                        DevReadBlock(indirect[k], direct);
                        for(i=0;i<4;i++)
                        {
                            if(!strcmp(direct[i].name, dirname[path_num-1]))
                            {
                                free(inode); free(direct);
                                return -1;}
                            if(direct[i].name[0] == 0){
                                strcpy(direct[i].name, dirname[path_num-1]);
                                direct[i].inodeNum = inodeno;
                                DevWriteBlock(indirect[k], direct);
                                k=16; break;
                            }
                        }
                    }
                    if(k<16)
                    {
                        int bf = GetFreeBlockNum();
                        SetBlockBitmap(bf);
                        //            printf("indirect index blk num : %d, k:%d\n", bf,k);
                        indirect[k] = bf;
                        DevWriteBlock(inode->indirBlockPtr, indirect);

                        memset(direct, 0, BLOCK_SIZE);
                        strcpy(direct[0].name, dirname[path_num-1]);
                        direct[0].inodeNum = inodeno;
                        DevWriteBlock(bf, direct);
                    }
                }
            }
        }
    }

    SetInodeBitmap(inodeno);

    memset(direct, 0, BLOCK_SIZE);
    blkno = GetFreeBlockNum();
    SetBlockBitmap(blkno);
    //    printf("%s block num : %d\n",dirname[path_num-1], blkno);
    strcpy(direct[0].name, ".");
    strcpy(direct[1].name, "..");
    direct[0].inodeNum = inodeno;
    direct[1].inodeNum = parent_inodeno;
    DevWriteBlock(blkno, (char*)direct);
    memset(inode, 0, sizeof(Inode));
    inode->type = FILE_TYPE_DIR; //enum
    inode->dirBlockPtr[0] = blkno;
    inode->size = BLOCK_SIZE*2;
    PutInode(inodeno, inode);

    memset(inode, 0, sizeof(Inode));
    GetInode(inodeno, inode);
    (pFileSysInfo->numAllocBlocks)++;		// global
    (pFileSysInfo->numFreeBlocks)--;
    (pFileSysInfo->numAllocInodes)++;
    DevWriteBlock(0, pFileSysInfo);

    GetInode(0, inode);
    DevReadBlock(inode->dirBlockPtr[1], direct);
    GetInode(direct[0].inodeNum, inode);

    return 0; 
}


int		RemoveDir(const char* szDirName)
{
    int i,k, w, path_num;
    int blkno;
    int my_inodeno=0, indexno=0, parent_inodeno;
    char dirname[10][12];
    memset(dirname, 0, 10);

    path_num = getToken(szDirName, &dirname);

    Inode* inode = (Inode*)calloc(1,sizeof(Inode));
    DirEntry* direct = (DirEntry*)calloc(4,sizeof(DirEntry));
    int* indirect = NULL; 
    char flag = 0;

    for(k=0; k<path_num;k++){
        flag = 0;
        parent_inodeno = my_inodeno;
        GetInode(my_inodeno, inode); 
        if(inode->dirBlockPtr[0] && flag == 0){
            DevReadBlock(inode->dirBlockPtr[0], (char*)direct);
            for(i=0;i<4;i++)
            {
                if(!strcmp(direct[i].name, dirname[k])) //remove dir
                {
                    my_inodeno = direct[i].inodeNum;
                    blkno = inode->dirBlockPtr[0];
                    indexno = i;
                    flag = 1;
                    break;
                }
            }
        }
        if(inode->dirBlockPtr[1] && flag == 0)
        {
            memset(direct, 0, BLOCK_SIZE);
            DevReadBlock(inode->dirBlockPtr[1], (char*)direct);
            for(i=0;i<4;i++)
            {
                if(!strcmp(direct[i].name, dirname[k]))
                {
                    my_inodeno = direct[i].inodeNum;
                    blkno = inode->dirBlockPtr[1];
                    indexno = i;
                    flag = 1;
                    break;
                }
            }
        }
        if(inode->indirBlockPtr && flag == 0){
            indirect = (int*)calloc(16, 4);
            DevReadBlock(inode->indirBlockPtr, (char*)indirect);
            for(w=0;w<16;w++){
                memset(direct, 0, BLOCK_SIZE);
                DevReadBlock(indirect[w], (char*)direct);
                for(i=0;i<4;i++)
                    if(!strcmp(direct[i].name, dirname[k]))
                    {
                        my_inodeno = direct[i].inodeNum;
                        blkno = indirect[w];
                        indexno = i;
                        flag = 1;
                        break;
                    }
                if(flag==1)break;
            }
        }
    }
    if(flag == 0) return -1;

    /*reset dir entry*/
    if(indexno == 0 && strcmp(direct[0].name, ".")) //"."가 아니고 인덱스 번호가 0번일때
    {
        ResetBlockBitmap(blkno);
        if(indirect != NULL){
            if(w==0){
                memset(indirect, 0, BLOCK_SIZE);
                DevWriteBlock(inode->indirBlockPtr, indirect);
                ResetBlockBitmap(inode->indirBlockPtr);
                inode->indirBlockPtr = 0;
            }
            else{
                indirect[w] = 0;
                DevWriteBlock(inode->indirBlockPtr, indirect);}
        }
        else if(inode->dirBlockPtr[1] == parent_inodeno)
        {
            inode->dirBlockPtr[1]=0;
        }
    }
    strcpy(direct[indexno].name, "");
    direct[indexno].inodeNum = 0;
    DevWriteBlock(blkno, direct);
    inode->size -= BLOCK_SIZE;
    PutInode(parent_inodeno, inode);

    /*remove*/ //i = 상위 안의 dir entry index
    GetInode(my_inodeno, inode);
    blkno = inode->dirBlockPtr[0];
    ResetBlockBitmap(blkno);
    memset(direct, 0, BLOCK_SIZE);
    DevWriteBlock(blkno, direct);
    ResetInodeBitmap(my_inodeno);
    //    printf("reset inodeno :%d\n", my_inodeno);    	
    memset(inode, 0, sizeof(Inode));
    PutInode(my_inodeno, inode);
    (pFileSysInfo->numAllocBlocks)--;	
    (pFileSysInfo->numFreeBlocks)++;
    (pFileSysInfo->numAllocInodes)--;
    DevWriteBlock(0, pFileSysInfo);

    free(direct); free(inode);
    if(indirect != NULL) free(indirect);
    return 0;
}


int		EnumerateDirStatus(const char* szDirName, DirEntryInfo* pDirEntry, int dirEntrys)
{
    /*Get Token*/
    int i,k, w, path_num;
    char dirname[10][12];
    memset(dirname, 0, 10);

    path_num = getToken(szDirName, &dirname);

    /*Get Inode Block, Dir Entry Block*/
    Inode* inode = (Inode*)calloc(1, sizeof(Inode));
    DirEntry* direct = (DirEntry*)calloc(4,sizeof(DirEntry));
    int* indirect = NULL;
    char flag = 0;

    GetInode(0, inode);
    if(path_num == 1) {
        DevReadBlock(inode->dirBlockPtr[0], (char*)direct);
    }
    for(k=0; k<path_num;k++){
        flag = 0;
        if(inode->dirBlockPtr[0] && flag == 0){
            DevReadBlock(inode->dirBlockPtr[0], (char*)direct);
            for(i=0;i<4;i++)
            {
                if(!strcmp(direct[i].name, dirname[k]))
                {
                    memset(inode, 0, sizeof(Inode));
                    GetInode(direct[i].inodeNum, inode);
                    memset(direct, 0, BLOCK_SIZE);
                    flag = 1;
                    break;
                }
            }
        }
        if(inode->dirBlockPtr[1] && flag == 0)
        {
            memset(direct, 0, BLOCK_SIZE);
            DevReadBlock(inode->dirBlockPtr[1], (char*)direct);
            for(i=0;i<4;i++)
            {
                if(!strcmp(direct[i].name, dirname[k]))
                {
                    memset(inode, 0, sizeof(Inode));
                    GetInode(direct[i].inodeNum, inode);
                    flag = 1;
                    break;
                }
            }
        }
        if(inode->indirBlockPtr && flag == 0){
            indirect = (int*)calloc(16, 4);
            DevReadBlock(inode->indirBlockPtr, (char*)indirect);
            for(w=0;w<16;w++){
                memset(direct, 0, BLOCK_SIZE);
                DevReadBlock(indirect[w], (char*)direct);
                for(i=0;i<4;i++)
                    if(!strcmp(direct[i].name, dirname[k]))
                    {
                        memset(inode, 0, sizeof(Inode));
                        GetInode(direct[i].inodeNum, inode);
                        //            DevReadBlock(inode->dirBlockPtr[0], (char*)direct);
                        memset(direct, 0, BLOCK_SIZE);
                        flag = 1;
                        break;
                    }
            }
        }
    }
    /*dir entry info save to parameter*/
    int inodesize = inode->size;
    for(i = 0; i<dirEntrys; i++){
        if(i % 4 ==0){
            memset(direct, 0, BLOCK_SIZE);
            if(i < 8)
                DevReadBlock(inode->dirBlockPtr[(i/4)], direct);
            else {
                if(i == 8)
                {	
                    indirect = (int*)calloc(16,sizeof(int));
                    DevReadBlock(inode->indirBlockPtr, (char*)indirect);
                }
                DevReadBlock(indirect[(i-8)/4],direct);
            }
        }
        if(direct[i%4].name[0] == 0) 
        {
            if(inodesize <= 0){
                free(inode);free(direct);
                if(indirect != NULL) free(indirect);
                return i;}
            else
            {
                char* namebf = (char*)calloc(MAX_NAME_LEN, 1);
                strcpy(pDirEntry[i].name, namebf);
                free(namebf);
            }
        }
        else {
            strcpy(pDirEntry[i].name, direct[i%4].name);
            inodesize -= BLOCK_SIZE;
        }
        Inode* bf = (Inode*)calloc(1,sizeof(Inode));
        pDirEntry[i].inodeNum = direct[i%4].inodeNum;
        GetInode(pDirEntry[i].inodeNum, bf);
        pDirEntry[i].type = bf->type;
        free(bf);
    }

    if(indirect != NULL) free(indirect);
    free(inode); free(direct);
    return i;
}

void FileSysInit(void) //초기화
{
    char *buffer = (char*)calloc(BLOCK_SIZE,sizeof(char));
    int i;
    if(buffer == NULL)
    {	
        printf("memory allocation ERROR\n");
        return;
    }

    DevCreateDisk();
    for(i=0; i<512;i++)
        DevWriteBlock(i, buffer);

    free(buffer);
}
void SetInodeBitmap(int inodeno)
{
    char *bitmap = (char*)calloc(BLOCK_SIZE, sizeof(char));
    DevReadBlock(INODE_BITMAP_BLOCK_NUM, bitmap);
    char bit = 0x80 >> (inodeno%8);
    bitmap[inodeno / 8] |= bit;
    DevWriteBlock(INODE_BITMAP_BLOCK_NUM, bitmap);
    if(inodeno%4 == 0)
        SetBlockBitmap(inodeno/4+INODELIST_BLOCK_FIRST);

    free(bitmap);
}


void ResetInodeBitmap(int inodeno)
{
    char *bitmap = (char*)calloc(BLOCK_SIZE, sizeof(char));
    DevReadBlock(INODE_BITMAP_BLOCK_NUM, bitmap);
    char bit = 0x80 >> (inodeno%8);
    bit = ~bit;
    bitmap[inodeno/8] &= bit;
    DevWriteBlock(INODE_BITMAP_BLOCK_NUM, bitmap);

    char shift; int i;
    char flag = 0;
    if(inodeno%8 >3) shift = 0x08;
    else shift = 0x80;

    for(i=0;i<4;i++)
    {
        if((bitmap[inodeno/8] & shift)!=0)
            flag = 1;
        shift >> 1;
    }
    if(flag == 0)
        ResetBlockBitmap(inodeno/4 + INODELIST_BLOCK_FIRST);


    free(bitmap);
}


void SetBlockBitmap(int blkno) //0 ~
{
    char *bitmap = (char*)calloc(BLOCK_SIZE, sizeof(char));
    DevReadBlock(BLOCK_BITMAP_BLOCK_NUM, bitmap);
    char bit = 0x80 >> (blkno%8);
    bitmap[blkno / 8] |= bit;
    DevWriteBlock(BLOCK_BITMAP_BLOCK_NUM, bitmap);

    free(bitmap);
}


void ResetBlockBitmap(int blkno)
{
    char *bitmap = (char*)calloc(BLOCK_SIZE, sizeof(char));
    DevReadBlock(BLOCK_BITMAP_BLOCK_NUM, bitmap);
    char bit = 0x80 >> (blkno%8);
    bit = ~bit;
    bitmap[blkno/8] &= bit;
    DevWriteBlock(BLOCK_BITMAP_BLOCK_NUM, bitmap);

    free(bitmap);
}


void PutInode(int inodeno, Inode* pInode)
{
    Inode *buffer = (Inode*)calloc(4, sizeof(Inode));
    int i=0;
    int anoinode=0;
    DevReadBlock(inodeno/NUM_OF_INODE_PER_BLOCK + INODELIST_BLOCK_FIRST, buffer);
    if(inodeno >= NUM_OF_INODE_PER_BLOCK) anoinode = inodeno%NUM_OF_INODE_PER_BLOCK;
    else anoinode = inodeno;

    buffer[anoinode].size = pInode->size;
    buffer[anoinode].type = pInode->type;
    buffer[anoinode].dirBlockPtr[0] = pInode->dirBlockPtr[0];
    buffer[anoinode].dirBlockPtr[1] = pInode->dirBlockPtr[1];
    buffer[anoinode].indirBlockPtr = pInode->indirBlockPtr;

    DevWriteBlock(inodeno/4 + INODELIST_BLOCK_FIRST, buffer);
    free(buffer);
}

///cursor
void GetInode(int inodeno, Inode* pInode)
{
    Inode *buffer = (Inode*)calloc(4, sizeof(Inode));
    int i;
    DevReadBlock(inodeno/NUM_OF_INODE_PER_BLOCK + INODELIST_BLOCK_FIRST, buffer);
    int bf=0;
    if(inodeno >= NUM_OF_INODE_PER_BLOCK) inodeno = inodeno%NUM_OF_INODE_PER_BLOCK;

    pInode->size = buffer[inodeno].size;
    pInode->type = buffer[inodeno].type;
    pInode->dirBlockPtr[0] = buffer[inodeno].dirBlockPtr[0];
    pInode->dirBlockPtr[1] = buffer[inodeno].dirBlockPtr[1];
    pInode->indirBlockPtr = buffer[inodeno].indirBlockPtr;

    free(buffer);
}


int GetFreeInodeNum(void)
{
    int i=0;
    char check = 0x80; //젤 앞이 0번이라고 가정
    char *bitmap = (char*)calloc(BLOCK_SIZE, sizeof(char));
    DevReadBlock(INODE_BITMAP_BLOCK_NUM, bitmap);
    for(i=0;i<BLOCK_SIZE*8;i++){
        check = 0x80 >> (i%8);
        if((check & bitmap[i/8])==0)	break;
    }

    free(bitmap);
    return i;
}


int GetFreeBlockNum(void)
{
    int i=0;
    char check = 0x80; //젤 앞이 0번이라고 가정
    char *bitmap = (char*)calloc(BLOCK_SIZE, sizeof(char));
    DevReadBlock(BLOCK_BITMAP_BLOCK_NUM, bitmap);
    for(i=19;i<BLOCK_SIZE*8;i++){
        check = 0x80 >> (i%8);
        if((check & bitmap[i/8])==0)	break;
    }

    free(bitmap);
    return i;
}

