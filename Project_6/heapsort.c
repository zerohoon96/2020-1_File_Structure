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

	inputfp=fopen(argv[2],"r"); //input file�� �б� ��������, output file�� ���� �������� fopen
	outputfp=fopen(argv[3],"w");
	
	readPage(inputfp,headerbuf,0); //header page�� �о ����
	writePage(outputfp,headerbuf,0); //header page�� �״�� output file�� write

	memcpy(&all_pages,headerbuf,4); //�� page ������ record ���� ����
	memcpy(&all_records,headerbuf+4,4);

	heaparray=(char**)malloc((all_pages-1)*sizeof(char*)); //����������� ������ heap ������ ���� 2���� �迭 ���� �Ҵ�
	for(int i=0;i<all_pages-1;i++){
		heaparray[i]=(char*)malloc(PAGE_SIZE*sizeof(char));
		memset(heaparray[i],(char)0xff,PAGE_SIZE);
	}

	buildHeap(inputfp,heaparray); //heap�� build
	makeSortedFile(outputfp,heaparray); //������� heap���� ���� ���� ����

	for(int i=0;i<all_pages-1;i++) //�迭 �Ҵ�����
		free(heaparray[i]);
	free(heaparray);

	fclose(inputfp);
	fclose(outputfp);
	return 1;
}

void readPage(FILE *fp, char *pagebuf, int pagenum){ //���ۿ� ������ ��ȣ�� ���޹޾Ƽ� ������ �������� �����͸� ���ۿ� �����ϴ� �Լ�
	fseek(fp,pagenum*PAGE_SIZE,SEEK_SET); //�ش� page�� offset �̵� �� fread
	fread(pagebuf,PAGE_SIZE,1,fp);
}

void writePage(FILE *fp, const char *pagebuf, int pagenum){ //�����Ϳ� ������ ��ȣ�� ���޹޾Ƽ� ������ �������� write�� �����ϴ� �Լ� 
	fseek(fp,pagenum*PAGE_SIZE,SEEK_SET); //�ش� page�� offset �̵� �� fwrite
	fwrite(pagebuf,PAGE_SIZE,1,fp);
}

void buildHeap(FILE *inputfp, char **heaparray){ //heap�� build�ϴ� �Լ�
	char pagebuf[PAGE_SIZE],recordbuf[RECORD_SIZE],tmp[RECORD_SIZE];
	char *key,*comparekey;
	int count=0,isChanged=0,len;
	int parent,node,start;
	int pagenum,recordnum;
	int route[10];	
	
	for(int i=1;i<all_pages;i++){ //header page�� ������ ������ page�� ���ʷ� �����鼭 heap�� ����
		readPage(inputfp,pagebuf,i); //page read
		
		for(int j=0;j<RECORD_PER_PAGE;j++){ //record�� �ݺ��ϸ� key�� ����
			if(count==all_records)
				break; //��� record�� �о����� break

			memcpy(recordbuf,pagebuf+(j*RECORD_SIZE),RECORD_SIZE); //heap �迭�� �߰��ϱ� ���� record�� ����

			if(count!=0){ //ù ��尡 �ƴ� �� �˻� ��带 ����
				node=count;
				for(int k=0;k<10;k++){ //��� ��θ� ����
					parent=node/2; //�θ� ��带 ����
					if(node%2==0)parent-=1;

					route[k]=parent; //��Ʈ�� �θ��带 ����
					if(parent==0){ //��Ʈ������ �ö� ��� break;
						start=k;
						break;
					}
					else
						node=parent; //�θ� ���� �ö�
				}
			}
			else{ //ó���̸� ó���� �����ϰ� ���� ���ڵ� Ž��
				memcpy(heaparray[0],recordbuf,RECORD_SIZE);
				count++;
				continue;
			}
			
			char pagebuf2[PAGE_SIZE];
			memcpy(pagebuf2,pagebuf,PAGE_SIZE);
			
			key=strtok(pagebuf2+(j*RECORD_SIZE),"#"); //������ key�� ����
			for(int k=start;k>=0;k--){ //root���� ��ο� ���� ���ϸ鼭 swap
				pagenum=route[k]/RECORD_PER_PAGE;
				recordnum=route[k]%RECORD_PER_PAGE;
				
				char tmp[RECORD_SIZE];

				memcpy(tmp,heaparray[pagenum]+recordnum*RECORD_SIZE,RECORD_SIZE); //���� key ������ ���� ���ڵ带 ����
				for(len=0;len<14;len++){ //���� key�� �� ���̸� ����
					if(tmp[len]=='#')break;
				}
				comparekey=strtok(tmp,"#"); 
				if(atof(key)<atof(comparekey)){ //��λ��� key���� ���� ��� swap

					if(isChanged==1) //�̹� ����� ��� ���� �Ҵ�� �޸𸮸� free�ϰ� ���ο� ������ �Ҵ�
						free(key);
					key=(char*)malloc(sizeof(char)*len);
					strncpy(key,comparekey,len);

					memcpy(tmp,recordbuf,RECORD_SIZE);
					memcpy(recordbuf,heaparray[pagenum]+recordnum*RECORD_SIZE,RECORD_SIZE); //�ٲ� key�� �����͸� ����
					memcpy(heaparray[pagenum]+RECORD_SIZE*recordnum,tmp,RECORD_SIZE); //heap �迭�� �ٲ� �����͸� ����
					
				}
			}
			memcpy(heaparray[i-1]+RECORD_SIZE*(count%RECORD_PER_PAGE),recordbuf,RECORD_SIZE); //���� �ڸ��� ������ ����
			count++;
			if(isChanged==1){ //����� ���·� ���� ��� �ʱ� ���·� �ǵ���
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

	for(int i=0;i<all_records;i++){ //��ü ���ڵ忡 ���ؼ� �ݺ� ����

		if(i%RECORD_PER_PAGE==0){ //������ ���۰� ������ ��� ��������� �������� ���ʷ� write
			if(i>0)
				writePage(outputfp,pagebuf,i/RECORD_PER_PAGE);
			memset(pagebuf,(char)0xff,PAGE_SIZE);
		}

		memcpy(pagebuf+RECORD_SIZE*(i%RECORD_PER_PAGE),heaparray[0],RECORD_SIZE); //��� ����� ���ڵ带 ������ ���ۿ� pop
		memcpy(heaparray[0],heaparray[lastRecord/RECORD_PER_PAGE]+RECORD_SIZE*(lastRecord%RECORD_PER_PAGE),RECORD_SIZE); //������ ��带 ��Ʈ ���� ����
		memcpy(recordbuf,heaparray[0],RECORD_SIZE); //���ο� ��Ʈ ����� record ����
		memcpy(tmp,recordbuf,RECORD_SIZE); //�ڸ��� �˻��� ���ο� ��Ʈ ����� key�� ����
		key=strtok(tmp,"#");
		lastRecord--;

		while(Lchild<=lastRecord||Rchild<=lastRecord){ //���� ������Ʈ�� ��Ʈ ��忡 ���� �˻� ����
			isChanged=0;
			memset(tmp2,(char)0xff,RECORD_SIZE);
			if(Lchild<=lastRecord){ //���� �ڽ��� ������ key�� ���ϰ�, ���� ����� key�� ��
				memcpy(tmp2,heaparray[Lchild/RECORD_PER_PAGE]+RECORD_SIZE*(Lchild%RECORD_PER_PAGE),RECORD_SIZE);
				Lkey=strtok(tmp2,"#");
				if(atof(key)>atof(Lkey)){ //���� ���� ���ؼ� ��ȯ�� �ʿ��� ���, ��ȯ�ϰ� ���� ���� ǥ��(1)
					isChanged=1;
				}
			}
			
			if(Rchild<=lastRecord){ //������ �ڽ��� ������ key�� ���ϰ�, ���� ����� key�� ��
				memcpy(tmp3,heaparray[Rchild/RECORD_PER_PAGE]+RECORD_SIZE*(Rchild%RECORD_PER_PAGE),RECORD_SIZE);
				Rkey=strtok(tmp3,"#");
				if(atof(key)>atof(Rkey)){ //���� ���� ���ؼ� ��ȯ�� �ʿ��� ���, ��ȯ�ϰ� ������ ���� ǥ��(2)
					if((isChanged==1&&atof(Lkey)>atof(Rkey))||isChanged==0){
						isChanged=2;
					}
				}
			}

			char tmp4[RECORD_SIZE];
			pagenum=cur/RECORD_PER_PAGE;
			recordnum=cur%RECORD_PER_PAGE;
			memcpy(tmp4,heaparray[pagenum]+RECORD_SIZE*recordnum,RECORD_SIZE);
			if(isChanged==0) //������ �ʿ䰡 ���� ��� ����
				break;
			
			else if(isChanged==1){ //���ʰ� swap�� ��� �����͸� swap�ϰ� ���� ��带 �ٲ�
				memcpy(heaparray[pagenum]+RECORD_SIZE*recordnum,heaparray[Lchild/RECORD_PER_PAGE]+RECORD_SIZE*(Lchild%RECORD_PER_PAGE),RECORD_SIZE);
				memcpy(heaparray[Lchild/RECORD_PER_PAGE]+RECORD_SIZE*(Lchild%RECORD_PER_PAGE),tmp4,RECORD_SIZE);
				cur=Lchild;
			}
			else{ //�����ʰ� swap�� ��� �����͸� swap�ϰ� ���� ��带 �ٲ�
				memcpy(heaparray[pagenum]+RECORD_SIZE*recordnum,heaparray[Rchild/RECORD_PER_PAGE]+RECORD_SIZE*(Rchild%RECORD_PER_PAGE),RECORD_SIZE);
				memcpy(heaparray[Rchild/RECORD_PER_PAGE]+RECORD_SIZE*(Rchild%RECORD_PER_PAGE),tmp4,RECORD_SIZE);
				cur=Rchild;
			}
			Lchild=cur*2+1; //���ο� �ڽ� ��带 ������Ʈ
			Rchild=cur*2+2;
		}
		Lchild=1; //����, ������ �ڽ� ���� �ֱ� ��� ������ �ʱ�ȭ 
		Rchild=2;
		cur=0;
	}
	writePage(outputfp,pagebuf,all_pages-1); //������ ���� �������� write
}
