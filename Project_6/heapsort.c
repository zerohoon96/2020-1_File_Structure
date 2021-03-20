#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "person.h"

#define RECORD_PER_PAGE (PAGE_SIZE/RECORD_SIZE)

void readPage(FILE*,char*,int);
void writePage(FILE*,const char*,int);
void buildHeap(FILE*,char**);
void makeSortedFile(FILE*,char**);

int all_pages,all_records;

int main(int argc, char *argv[]){
	FILE *inputfp;
	FILE *outputfp;
	char **heaparray;
	char headerbuf[PAGE_SIZE];

	inputfp=fopen(argv[2],"r"); //input file은 읽기 전용으로, output file은 쓰기 전용으로 fopen
	outputfp=fopen(argv[3],"w");
	
	readPage(inputfp,headerbuf,0); //header page를 읽어서 저장
	writePage(outputfp,headerbuf,0); //header page를 그대로 output file에 write

	memcpy(&all_pages,headerbuf,4); //총 page 개수와 record 개수 저장
	memcpy(&all_records,headerbuf+4,4);

	heaparray=(char**)malloc((all_pages-1)*sizeof(char*)); //헤더페이지를 제외한 heap 저장을 위한 2차원 배열 동적 할당
	for(int i=0;i<all_pages-1;i++){
		heaparray[i]=(char*)malloc(PAGE_SIZE*sizeof(char));
		memset(heaparray[i],(char)0xff,PAGE_SIZE);
	}

	buildHeap(inputfp,heaparray); //heap을 build
	makeSortedFile(outputfp,heaparray); //만들어진 heap으로 정렬 파일 생성

	for(int i=0;i<all_pages-1;i++) //배열 할당해제
		free(heaparray[i]);
	free(heaparray);

	fclose(inputfp);
	fclose(outputfp);
	return 1;
}

void readPage(FILE *fp, char *pagebuf, int pagenum){ //버퍼와 페이지 번호를 전달받아서 지정한 페이지의 데이터를 버퍼에 저장하는 함수
	fseek(fp,pagenum*PAGE_SIZE,SEEK_SET); //해당 page로 offset 이동 후 fread
	fread(pagebuf,PAGE_SIZE,1,fp);
}

void writePage(FILE *fp, const char *pagebuf, int pagenum){ //데이터와 페이지 번호를 전달받아서 지정한 페이지에 write를 수행하는 함수 
	fseek(fp,pagenum*PAGE_SIZE,SEEK_SET); //해당 page로 offset 이동 후 fwrite
	fwrite(pagebuf,PAGE_SIZE,1,fp);
}

void buildHeap(FILE *inputfp, char **heaparray){ //heap을 build하는 함수
	char pagebuf[PAGE_SIZE],recordbuf[RECORD_SIZE],tmp[RECORD_SIZE];
	char *key,*comparekey;
	int count=0,isChanged=0,len;
	int parent,node,start;
	int pagenum,recordnum;
	int route[10];	
	
	for(int i=1;i<all_pages;i++){ //header page를 제외한 나머지 page를 차례로 읽으면서 heap을 생성
		readPage(inputfp,pagebuf,i); //page read
		
		for(int j=0;j<RECORD_PER_PAGE;j++){ //record를 반복하며 key를 추출
			if(count==all_records)
				break; //모든 record를 읽었으면 break

			memcpy(recordbuf,pagebuf+(j*RECORD_SIZE),RECORD_SIZE); //heap 배열에 추가하기 위한 record를 저장

			if(count!=0){ //첫 노드가 아닐 때 검사 노드를 설정
				node=count;
				for(int k=0;k<10;k++){ //모든 경로를 추적
					parent=node/2; //부모 노드를 저장
					if(node%2==0)parent-=1;

					route[k]=parent; //루트에 부모노드를 저장
					if(parent==0){ //루트노드까지 올라간 경우 break;
						start=k;
						break;
					}
					else
						node=parent; //부모 노드로 올라감
				}
			}
			else{ //처음이면 처음에 저장하고 다음 레코드 탐색
				memcpy(heaparray[0],recordbuf,RECORD_SIZE);
				count++;
				continue;
			}
			
			char pagebuf2[PAGE_SIZE];
			memcpy(pagebuf2,pagebuf,PAGE_SIZE);
			
			key=strtok(pagebuf2+(j*RECORD_SIZE),"#"); //삽입할 key를 추출
			for(int k=start;k>=0;k--){ //root부터 경로에 따라 비교하면서 swap
				pagenum=route[k]/RECORD_PER_PAGE;
				recordnum=route[k]%RECORD_PER_PAGE;
				
				char tmp[RECORD_SIZE];

				memcpy(tmp,heaparray[pagenum]+recordnum*RECORD_SIZE,RECORD_SIZE); //비교할 key 추출을 위해 레코드를 꺼냄
				for(len=0;len<14;len++){ //비교할 key와 그 길이를 저장
					if(tmp[len]=='#')break;
				}
				comparekey=strtok(tmp,"#"); 
				if(atof(key)<atof(comparekey)){ //경로상의 key보다 작은 경우 swap

					if(isChanged==1) //이미 변경된 경우 먼저 할당된 메모리를 free하고 새로운 공간을 할당
						free(key);
					key=(char*)malloc(sizeof(char)*len);
					strncpy(key,comparekey,len);

					memcpy(tmp,recordbuf,RECORD_SIZE);
					memcpy(recordbuf,heaparray[pagenum]+recordnum*RECORD_SIZE,RECORD_SIZE); //바뀐 key의 데이터를 저장
					memcpy(heaparray[pagenum]+RECORD_SIZE*recordnum,tmp,RECORD_SIZE); //heap 배열에 바뀐 데이터를 저장
					
				}
			}
			memcpy(heaparray[i-1]+RECORD_SIZE*(count%RECORD_PER_PAGE),recordbuf,RECORD_SIZE); //최종 자리에 데이터 저장
			count++;
			if(isChanged==1){ //변경된 상태로 끝난 경우 초기 상태로 되돌림
				free(key);
				isChanged=0;
			}
		}
	}
}

void makeSortedFile(FILE *outputfp, char **heaparray){
	char pagebuf[PAGE_SIZE],tmp[RECORD_SIZE],tmp2[RECORD_SIZE],tmp3[RECORD_SIZE],recordbuf[RECORD_SIZE];
	char *key,*Lkey,*Rkey;
	int lastRecord=all_records-1;
	int Lchild=1,Rchild=2;
	int cur=0,isChanged=0;
	int pagenum,recordnum;

	for(int i=0;i<all_records;i++){ //전체 레코드에 대해서 반복 실행

		if(i%RECORD_PER_PAGE==0){ //페이지 버퍼가 가득찬 경우 헤더페이지 다음부터 차례로 write
			if(i>0)
				writePage(outputfp,pagebuf,i/RECORD_PER_PAGE);
			memset(pagebuf,(char)0xff,PAGE_SIZE);
		}

		memcpy(pagebuf+RECORD_SIZE*(i%RECORD_PER_PAGE),heaparray[0],RECORD_SIZE); //루드 노드의 레코드를 페이지 버퍼에 pop
		memcpy(heaparray[0],heaparray[lastRecord/RECORD_PER_PAGE]+RECORD_SIZE*(lastRecord%RECORD_PER_PAGE),RECORD_SIZE); //마지막 노드를 루트 노드로 복사
		memcpy(recordbuf,heaparray[0],RECORD_SIZE); //새로운 루트 노드의 record 저장
		memcpy(tmp,recordbuf,RECORD_SIZE); //자리를 검사할 새로운 루트 노드의 key를 저장
		key=strtok(tmp,"#");
		lastRecord--;

		while(Lchild<=lastRecord||Rchild<=lastRecord){ //새로 업데이트된 루트 노드에 대한 검사 실행
			isChanged=0;
			memset(tmp2,(char)0xff,RECORD_SIZE);
			if(Lchild<=lastRecord){ //왼쪽 자식이 있으면 key를 구하고, 현재 노드의 key와 비교
				memcpy(tmp2,heaparray[Lchild/RECORD_PER_PAGE]+RECORD_SIZE*(Lchild%RECORD_PER_PAGE),RECORD_SIZE);
				Lkey=strtok(tmp2,"#");
				if(atof(key)>atof(Lkey)){ //현재 노드와 비교해서 교환이 필요한 경우, 교환하고 왼쪽 변경 표시(1)
					isChanged=1;
				}
			}
			
			if(Rchild<=lastRecord){ //오른쪽 자식이 있으면 key를 구하고, 현재 노드의 key와 비교
				memcpy(tmp3,heaparray[Rchild/RECORD_PER_PAGE]+RECORD_SIZE*(Rchild%RECORD_PER_PAGE),RECORD_SIZE);
				Rkey=strtok(tmp3,"#");
				if(atof(key)>atof(Rkey)){ //현재 노드와 비교해서 교환이 필요한 경우, 교환하고 오른쪽 변경 표시(2)
					if((isChanged==1&&atof(Lkey)>atof(Rkey))||isChanged==0){
						isChanged=2;
					}
				}
			}

			char tmp4[RECORD_SIZE];
			pagenum=cur/RECORD_PER_PAGE;
			recordnum=cur%RECORD_PER_PAGE;
			memcpy(tmp4,heaparray[pagenum]+RECORD_SIZE*recordnum,RECORD_SIZE);
			if(isChanged==0) //변경할 필요가 없는 경우 종료
				break;
			
			else if(isChanged==1){ //왼쪽과 swap한 경우 데이터를 swap하고 현재 노드를 바꿈
				memcpy(heaparray[pagenum]+RECORD_SIZE*recordnum,heaparray[Lchild/RECORD_PER_PAGE]+RECORD_SIZE*(Lchild%RECORD_PER_PAGE),RECORD_SIZE);
				memcpy(heaparray[Lchild/RECORD_PER_PAGE]+RECORD_SIZE*(Lchild%RECORD_PER_PAGE),tmp4,RECORD_SIZE);
				cur=Lchild;
			}
			else{ //오른쪽과 swap한 경우 데이터를 swap하고 현재 노드를 바꿈
				memcpy(heaparray[pagenum]+RECORD_SIZE*recordnum,heaparray[Rchild/RECORD_PER_PAGE]+RECORD_SIZE*(Rchild%RECORD_PER_PAGE),RECORD_SIZE);
				memcpy(heaparray[Rchild/RECORD_PER_PAGE]+RECORD_SIZE*(Rchild%RECORD_PER_PAGE),tmp4,RECORD_SIZE);
				cur=Rchild;
			}
			Lchild=cur*2+1; //새로운 자식 노드를 업데이트
			Rchild=cur*2+2;
		}
		Lchild=1; //왼쪽, 오른쪽 자식 노드와 최근 노드 정보를 초기화 
		Rchild=2;
		cur=0;
	}
	writePage(outputfp,pagebuf,all_pages-1); //마지막 남은 페이지를 write
}
