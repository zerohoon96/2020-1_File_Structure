#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include "sectormap.h"

typedef struct block { //block 구조체
	int block_num; //block의 번호를 저장하는 변수(free block이면 (*=(-1) )
	int page[PAGES_PER_BLOCK]; //page의 정보를 저장하는 배열(0:blank, 1:valid, -1:garbage)
	int garbage_count; //garbage ppn의 개수를 저장하는 변수
}BLOCK;

int dd_read(int ppn, char *pagebuf);
int dd_write(int ppn,char *pagebuf);
int dd_erase(int pbn);

void ftl_open();
void set_block();
void ftl_read(int lsn,char *sectorbuf);
void ftl_write(int lsn,char *sectorbuf);
void ftl_print();
int is_empty();
void overwrite(int lsn);
int find_garbage_block();
int find_free_block();
void update_real_data(int garbage_block,int free_block,int page_num);

extern FILE *flashfp;
int mapping_table[DATABLKS_PER_DEVICE*PAGES_PER_BLOCK];
char pagebuf[PAGE_SIZE];
BLOCK block[BLOCKS_PER_DEVICE]; //block의 수만큼 정보를 저장하는 구조체 배열

void ftl_open(){ //create address mapping table
	set_block(); //구조체 배열을 set
	for(int i=0;i<sizeof(mapping_table)/sizeof(int);i++){ //ppn을 초기화
		mapping_table[i]=-1;
	}
	return;
}

void set_block(){ //초기 block을 set

	for(int i=0;i<BLOCKS_PER_DEVICE;i++){ //block별로 초기 정보를 세팅
		for(int j=0;j<PAGES_PER_BLOCK;j++)
			block[i].page[j]=0;

		block[i].garbage_count=0;

		if(i==BLOCKS_PER_DEVICE-1){ //초기에는 마지막 block이 free block이므로 번호를 음수로 저장
			block[i].block_num=-(i+1);
		}
		else{
			block[i].block_num=i+1;
		}
	}
}

void ftl_read(int lsn, char *sectorbuf){
	int ppn;
	ppn=mapping_table[lsn];

	if(ppn==-1){
		printf("lsn(%d) is empty!\n",lsn);
		return;
	}

	dd_read(ppn,pagebuf);
	memcpy(sectorbuf,pagebuf,SECTOR_SIZE);

	return;
}


void ftl_write(int lsn, char *sectorbuf){ //write

	char garbage_pagebuf[PAGE_SIZE];
	int inputSize=strlen(sectorbuf);
	int ppn,pageIndex,data;
	int garbage_block,free_block;

	memset((void*)pagebuf,(char)0xFF,PAGE_SIZE); //pagebuf의 데이터를 세팅
	if(inputSize>SECTOR_SIZE) //sectorbuf의 길이가 512바이트를 넘어가는 경우 512바이트만 저장
		inputSize=SECTOR_SIZE;
	strncpy(pagebuf,sectorbuf,inputSize); //sector 부분에 sectorbuf의 내용 저장
	pagebuf[SECTOR_SIZE]=lsn; //spare 부분에 lsn 저장

	if((ppn=is_empty())>=0){ //비어 있는 ppn이 있는 경우
		if(mapping_table[lsn]!=-1){ //overwrite인 경우 (원래 ppn이 garbage가 됨)
			overwrite(lsn);
		}

		mapping_table[lsn]=ppn; //mapping_table 업데이트
		dd_write(ppn,pagebuf); //dd_write 실행
	}
	else{ //비어 있는 ppn이 없는 경우


		garbage_block=find_garbage_block(); //garbage block을 찾아서 저장
		free_block=find_free_block(); //free block을 찾아서 저장

		if(block[garbage_block].garbage_count==0){ //garbage ppn이 없는 경우 현재 block을 garbage block으로 지정
			garbage_block=mapping_table[lsn]/PAGES_PER_BLOCK;
			ppn=mapping_table[lsn]; //삭제 대상 ppn을 저장
		}

		if(mapping_table[lsn]!=-1) //overwrite인 경우 처리
			overwrite(lsn);

		(block[free_block].block_num)*=-1; //free block의 block_num 업데이트

		for(pageIndex=0;pageIndex<PAGES_PER_BLOCK;pageIndex++){ //free block의 page, file의 실제 데이터, mapping_table  업데이트
			data=block[garbage_block].page[pageIndex];

			if(data==1){ //garbage block의 ppn에서 유효한 ppn을 찾은 경우 free block에 복사

				update_real_data(garbage_block,free_block,pageIndex);
				block[free_block].page[pageIndex]=1;
			}
			block[garbage_block].page[pageIndex]=0;
		}

		dd_erase(block[garbage_block].block_num-1); //파일에서 garbage block erase

		(block[garbage_block].block_num)*=-1; //garbage_block을 새로운 free block로 설정
		block[garbage_block].garbage_count=0; //garbage_count을 0으로
		memset((void*)(block[garbage_block].page),0,PAGES_PER_BLOCK); //page 배열 초기화

		for(pageIndex=0;pageIndex<PAGES_PER_BLOCK;pageIndex++) //write할 page를 찾아서 ppn 계산
			if(block[free_block].page[pageIndex]==0)
				break;
		ppn=(block[free_block].block_num-1)*PAGES_PER_BLOCK+pageIndex;
		mapping_table[lsn]=ppn; //mapping_table 업데이트
		block[free_block].page[ppn%PAGES_PER_BLOCK]=1; //pages 배열에 정보 업데이트

		dd_write(ppn,pagebuf);
	}
	fseek(flashfp,0,SEEK_SET);
	return;
}

void ftl_print(){ //print address mapping table
	int free_block=find_free_block();
	printf("lpn ppn\n");
	for(int i=0;i<DATAPAGES_PER_DEVICE;i++)
		printf("%d %d\n",i,mapping_table[i]);
	
	printf("free block's pbn=%d\n",(block[free_block].block_num+1)*(-1));
	return;
}

int is_empty(){ //지정한 block 또는 page가 비어있는지 확인하는 함수
	for(int i=0;i<BLOCKS_PER_DEVICE;i++){ //전체 block을 탐색

		if(block[i].block_num<0) //free block인 경우 다음 block으로
			continue;

		for(int j=0;j<PAGES_PER_BLOCK;j++){ //block의 page 배열을 탐색
			if(block[i].page[j]==0){ //비어 있는 page를 찾은 경우 page 정보를 업데이트(valid 하므로 1), ppn을 리턴
				block[i].page[j]=1;
				return i*PAGES_PER_BLOCK+j;
			}
		}
	}

	return -1; //최종적으로 비어 있는 page를 찾지 못한 경우 -1 리턴
}

void overwrite(int lsn){ //overwrite인 경우 
	int blocknum,pagenum;
	char pagebuf[PAGE_SIZE];

	memset((void*)pagebuf,(char)0xFF,PAGE_SIZE); //pagebuf을 0xFF로 초기화

	blocknum=mapping_table[lsn]/PAGES_PER_BLOCK; //원래 ppn의 block, page 계산
	pagenum=mapping_table[lsn]%PAGES_PER_BLOCK;
	block[blocknum].page[pagenum]=-1;
	block[blocknum].garbage_count++;
}

int find_garbage_block(){ //garbage block을 찾는 함수
	int garbage_block=0;

	for(int i=1;i<BLOCKS_PER_DEVICE;i++){ //전체 block을 탐색
		if(block[i].block_num<0){ //free block이면 다음 block으로
			continue;
		}
		if(block[garbage_block].garbage_count<block[i].garbage_count)
			garbage_block=i;
	}
	return garbage_block; //최종 garbage_block 리턴
}

int find_free_block(){ //free block을 찾는 함수

	for(int i=0;i<BLOCKS_PER_DEVICE;i++){ //전체 block을 탐색
		if(block[i].block_num<0) //free block을 찾은 경우 리턴
			return i;
	}
}

void update_real_data(int garbage_block,int free_block,int page_num){ //garbage block의 유효 데이터를 free block으로 복사하는 함수
	char pagebuf[PAGE_SIZE];
	int garbage_pagenum,free_pagenum;

	garbage_pagenum=(block[garbage_block].block_num-1)*PAGES_PER_BLOCK+page_num; //garbage_block의 유효 데이터가 있는 ppn 계산
	free_pagenum=(block[free_block].block_num-1)*PAGES_PER_BLOCK+page_num; //free_block에 복사될 ppn 계산

	fseek(flashfp,garbage_pagenum*PAGE_SIZE,SEEK_SET); //pagebuf에 garbage_block의 유효 데이터 저장
	fread(pagebuf,PAGE_SIZE,1,flashfp);

	fseek(flashfp,free_pagenum*PAGE_SIZE,SEEK_SET); //free block의 ppn에 pagebuf 내용 저장
	fwrite(pagebuf,PAGE_SIZE,1,flashfp);

	for(int i=0;i<DATAPAGES_PER_DEVICE;i++){ //mapping table 업데이트
		if(mapping_table[i]==garbage_pagenum){
			mapping_table[i]=free_pagenum;
			break;
		}
	}
}
