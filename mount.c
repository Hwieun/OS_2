#include <stdio.h>
#include <stdlib.h>
#include "fs.h"
#include "disk.h"


FileSysInfo* pFileSysInfo = NULL;


void		Mount(MountType type)
{
	if(type == MT_TYPE_READWRITE)
    {
	    pFileSysInfo = (FileSysInfo*)calloc(1,sizeof(FileSysInfo));
        printf("malloc\n");
        DevOpenDisk();
        DevReadBlock(FILESYS_INFO_BLOCK, pFileSysInfo);
        printf("readwrite version mount\n");
        return;
    }

    FileSysInit();
	DirEntry *alloc = (DirEntry*)calloc(4, sizeof(DirEntry)); //할당 후 0으로 초기화
	int blockno = GetFreeBlockNum();
	int inodeno = GetFreeInodeNum();
    printf("inode:%d, blockno :%d\n", inodeno, blockno);
    strcpy(alloc[0].name, ".");
    printf("set root direntry name at mount : %s\n", alloc[0].name);
	alloc[0].inodeNum = inodeno;
	DevWriteBlock(blockno, alloc); //19
	
	pFileSysInfo = (FileSysInfo*)calloc(1,sizeof(FileSysInfo));
	pFileSysInfo->blocks = 512;
	pFileSysInfo->rootInodeNum = 0;
	pFileSysInfo->diskCapacity = FS_DISK_CAPACITY;
	pFileSysInfo->numAllocBlocks = 1;
	pFileSysInfo->numFreeBlocks = 511 - 19;
	pFileSysInfo->numAllocInodes = 1;
	pFileSysInfo->blockBitmapBlock = BLOCK_BITMAP_BLOCK_NUM;		
	pFileSysInfo->inodeBitmapBlock = INODE_BITMAP_BLOCK_NUM;
	pFileSysInfo->inodeListBlock = INODELIST_BLOCK_FIRST;
	pFileSysInfo->dataRegionBlock = INODELIST_BLOCK_FIRST + INODELIST_BLOCKS; //data block 시작 블록번호

	SetInodeBitmap(inodeno);
	SetBlockBitmap(blockno);
	SetBlockBitmap(0);
	SetBlockBitmap(1);
	SetBlockBitmap(2);
	SetBlockBitmap(3);
	
    Inode* pBuf = (Inode*)calloc(1,sizeof(Inode));
	GetInode(inodeno, pBuf);
	pBuf->dirBlockPtr[0] = blockno;
	pBuf->size = BLOCK_SIZE;
	pBuf->type = FILE_TYPE_DIR; 
    PutInode(inodeno, pBuf);

	free(pBuf);
	free(alloc);
}


void		Unmount(void)
{
    DevCloseDisk();	
	free(pFileSysInfo);
}


