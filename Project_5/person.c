#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "person.h"

#define RECORD_PER_PAGE (PAGE_SIZE/RECORD_SIZE)

void readPage(FILE*,char*,int);
void writePage(FILE*,const char*,int);
void insertInfo(int,Person*,char*);
void pack(char*,const Person*);
int unpack(char*,Person*);
void insert(FILE*,const Person*);
void delete(FILE*,const char*);
void insertInfo(int,Person*,char*);
void toBinary(char*,int);
int toDecimal(char*); 

int main(int argc, char *argv[]){ 
	FILE *fp; 
	char recordbuf[RECORD_SIZE];
	Person p;

	if((fp=fopen(argv[2],"r+"))==NULL)
		fp=fopen(argv[2],"w+");
	
	if(strcmp(argv[1],"i")==0){ //insert 옵션이 입력된 경우
		for(int i=0;i<6;i++)
			insertInfo(i,&p,argv[i+3]);
		insert(fp,&p);
	}
	else if(strcmp(argv[1],"d")==0){ //delete 옵션이 입력된 경우
		delete(fp,argv[3]);
	}
	else{ //유효하지 않은 옵션이 입력된 경우
		fprintf(stderr,"invalid option!\n");
		exit(1);
	}
	fclose(fp);
	return 0;
}

void readPage(FILE *fp, char *pagebuf, int pagenum){ //버퍼와 페이지 번호를 전달받아서 지정한 페이지의 데이터를 버퍼에 저장하는 함수
	fseek(fp,pagenum*PAGE_SIZE,SEEK_SET); //해당 page로 offset 이동 후 fread
	fread(pagebuf,PAGE_SIZE,1,fp);
}

void writePage(FILE *fp, const char *pagebuf, int pagenum){ //데이터와 페이지 번호를 전달받아서 지정한 페이지에 write를 수행하는 함수
	fseek(fp,pagenum*PAGE_SIZE,SEEK_SET); //해당 page로 offset 이동 후 fwrite
	fwrite(pagebuf,PAGE_SIZE,1,fp);
}

void pack(char *recordbuf, const Person *p){ //입력받은 정보를 저장할 형태로 pack해서 recordbuf에 저장하는 함수
	strcpy(recordbuf,p->sn);
	strcat(recordbuf,"#");
	strcat(recordbuf,p->name);
	strcat(recordbuf,"#");
	strcat(recordbuf,p->age);
	strcat(recordbuf,"#");
	strcat(recordbuf,p->addr);
	strcat(recordbuf,"#");
	strcat(recordbuf,p->phone);
	strcat(recordbuf,"#");
	strcat(recordbuf,p->email);
	strcat(recordbuf,"#");

	for(int i=0;i<RECORD_SIZE;i++)
		if(recordbuf[i]==0)recordbuf[i]=(char)0xff;
}

int unpack(char *recordbuf, Person *p){ //recordbuf의 내용을 unpack해서 p에 저장하는 함수
	char *buf;
	if(recordbuf[0]=='*'||recordbuf[0]==-1)return -1; //삭제된 레코드이거나, 비어있는 레코드인 경우 return -1
	
	buf=strtok(recordbuf,"#"); //#단위로 recordbuf을 잘라서 unpack
	for(int i=0;i<6;i++){
		insertInfo(i,p,buf);
		buf=strtok(NULL,"#");
	}
	return 1;
}

void insert(FILE *fp, const Person *p){ //Person record를 page에 추가하는 함수
	char recordbuf[RECORD_SIZE];
	char pagebuf[PAGE_SIZE];
	char headerbuf[PAGE_SIZE];
	char charbuf[4];
	int count=1;
	int pagenum,recordnum;
	int new_pagenum,new_recordnum;

	memset(recordbuf,0,RECORD_SIZE); //각 버퍼를 알맞은 값으로 초기화
	memset(headerbuf,(char)0xff,PAGE_SIZE);
	memset(pagebuf,(char)0xff,PAGE_SIZE);
	
	fseek(fp,0,SEEK_END);
	if(ftell(fp)==0){ //첫 record insert인 경우 header 초기 setting
		for(int i=0;i<4;i++){
			toBinary(charbuf,count--);
			
			for(int j=0;j<4;j++)
				headerbuf[4*i+j]=charbuf[j];
			if(count==-2)
				count++;
		}
		writePage(fp,headerbuf,0);
	}

	readPage(fp,headerbuf,0); //삭제된 record가 있는지 확인
	for(int i=0;i<4;i++)charbuf[i]=headerbuf[8+i];
	if((pagenum=toDecimal(charbuf))>=0){ //삭제된 record가 있는 경우
		readPage(fp,pagebuf,pagenum); //가장 직전에 삭제된 record의 page를 read
		
		for(int i=0;i<4;i++)charbuf[i]=headerbuf[12+i]; //가장 직전에 삭제된 recordnum을 저장
		recordnum=toDecimal(charbuf); 

		for(int i=0;i<4;i++)charbuf[i]=pagebuf[recordnum*RECORD_SIZE+1+i]; //header page에 update할 새로운 pagenum, recordnum을 저장
		new_pagenum=toDecimal(charbuf);
		for(int i=0;i<4;i++)charbuf[i]=pagebuf[recordnum*RECORD_SIZE+5+i];
		new_recordnum=toDecimal(charbuf);

		toBinary(charbuf,new_pagenum); //header page를 update
		for(int i=0;i<4;i++)headerbuf[8+i]=charbuf[i];
		toBinary(charbuf,new_recordnum);
		for(int i=0;i<4;i++)headerbuf[12+i]=charbuf[i];
		writePage(fp,headerbuf,0);

		pack(recordbuf,p);
		for(int i=0;i<RECORD_SIZE;i++)pagebuf[recordnum*RECORD_SIZE+i]=recordbuf[i]; //pagebuf에 record를 추가
		writePage(fp,pagebuf,pagenum); //update된 pagebuf를 write
	}
	else{ //삭제된 record가 없는 경우
		
		for(int i=0;i<4;i++)charbuf[i]=headerbuf[i]; //page 개수와 record 개수 저장
		pagenum=toDecimal(charbuf)-1;
		for(int i=0;i<4;i++)charbuf[i]=headerbuf[4+i];
		recordnum=toDecimal(charbuf);

		toBinary(charbuf,recordnum+1); //headerbuf의 record 부분에 recordnum을 1 더해서 update
		for(int i=0;i<4;i++)headerbuf[4+i]=charbuf[i];

		if(recordnum%RECORD_PER_PAGE==0/* &&recordnum/RECORD_PER_PAGE!=0*/){ //다음 page에 저장해야 하는 경우
			toBinary(charbuf,pagenum+2); //headerbuf의 page 부분에 pagenum을 1 더해서 update
			for(int i=0;i<4;i++)headerbuf[i]=charbuf[i];
			pagenum++;
			writePage(fp,pagebuf,pagenum); //다음 page 공간 할당
		}

		pack(recordbuf,p);
		writePage(fp,headerbuf,0); //header page를 update
		readPage(fp,pagebuf,pagenum); //업데이트할 page를 read
		recordnum%=RECORD_PER_PAGE; //record가 page의 몇 번째 record인지 저장
		strncpy(pagebuf+recordnum*RECORD_SIZE,recordbuf,RECORD_SIZE); //pagebuf에 새로운 record update
		writePage(fp,pagebuf,pagenum); //update된 pagebuf를 pagenum에 write
	}

}

void delete(FILE *fp, const char *sn){ //입력받은 주민번호를 가진 record를 찾아서 제거하는 함수
	char pagebuf[PAGE_SIZE];
	char headerbuf[PAGE_SIZE];
	char recordbuf[RECORD_SIZE];
	char charbuf[4];
	char deletebuf[8];
	int i,j;
	int pages,pagenum=-1,recordnum;

	Person p;

	readPage(fp,headerbuf,0); //header page를 read
	for(i=0;i<4;i++)charbuf[i]=headerbuf[i]; //page개수 저장
	pages=toDecimal(charbuf);
	for(i=1;i<pages;i++){ ///각 page를 탐색하면서 주민번호와 일치하는 record가 있는지 확인
		readPage(fp,pagebuf,i);
		for(j=0;j<RECORD_PER_PAGE;j++){ //page의 record를 탐색
			strncpy(recordbuf,pagebuf+RECORD_SIZE*j,RECORD_SIZE);

			if(unpack(recordbuf,&p)>0&&strcmp(p.sn,sn)==0){ //주민번호가 일치하는 target을 찾은 경우 pagenum, recordnum을 저장
				pagenum=i;
				recordnum=j;
				break;
			}
		}
		if(pagenum!=-1)break;
	}
	if(pagenum==-1){
		fprintf(stderr,"\"%s\" data is not exist\n",sn);
		exit(1);
	}

	readPage(fp,pagebuf,pagenum); //pagebuf에 target record가 있는 page를 read

	for(i=0;i<8;i++)deletebuf[i]=headerbuf[8+i]; //header page에 저장된 최근 삭제된 정보를 target record에 복사

	memset(recordbuf,(char)0xff,RECORD_SIZE);
	strcpy(recordbuf,"*"); //target record의 recordbuf를 set

	for(i=0;i<8;i++)recordbuf[1+i]=deletebuf[i]; //삭제 내용을 recordbuf에 update
	
	for(i=0;i<RECORD_SIZE;i++)pagebuf[RECORD_SIZE*recordnum+i]=recordbuf[i]; //target record가 있는 pagebuf를 update

	writePage(fp,pagebuf,pagenum); //update된 target record의 page를 write

	toBinary(charbuf,pagenum); //deletebuf에 target record의 pagenum과 recordnum을 update
	for(i=0;i<4;i++)deletebuf[i]=charbuf[i];
	toBinary(charbuf,recordnum);
	for(i=0;i<4;i++)deletebuf[4+i]=charbuf[i];

	for(i=0;i<8;i++)headerbuf[8+i]=deletebuf[i]; //headerbuf에 deletebuf 내용 update

	writePage(fp,headerbuf,0); //headerpage update
}

void insertInfo(int num,Person *p,char *buf){
	switch(num){
		case 0:
			strcpy(p->sn,buf);
			break;
		case 1:
			strcpy(p->name,buf);
			break;
		case 2:
			strcpy(p->age,buf);
			break;
		case 3:
			strcpy(p->addr,buf);
			break;
		case 4:
			strcpy(p->phone,buf);
			break;
		case 5:
			strcpy(p->email,buf);
			break;
	}
}


void toBinary(char *buf,int num){ //decimal 숫자를 binary로 바꿔서 buf에 저장
	char str1[8],str2[8];
	char n1,n2;
	int tmp=0,i,j;

	sprintf(str1,"%x",num); //입력받은 10진수를 16진수로 변환

	for(i=0;i<8;i++) //낮은 숫자를 오른쪽으로 이동
		if(str1[i]=='\0')
			break;
	for(j=0;j<8;j++){
		if(j<8-i)
			str2[j]='0';
		else
			str2[j]=str1[tmp++];
	}
	
	tmp=0;
	for(i=0;i<4;i++){ //1바이트씩 끊어서 char로 변환 후 buf에 저장
		n1=str2[i*2];
		n2=str2[i*2+1];
		if(n1>57){
			n1=tolower(n1);
			tmp+=(n1-87)*16;
		}
		else if(n1<=57)
			tmp+=16*(n1-48);
		if(n2>57){
			n2=tolower(n2);
			tmp+=n2-87;
		}
		else if(n2<=57)
			tmp+=n2-48;
		sprintf(buf+i,"%c",tmp);
		tmp=0;
	}
}

int toDecimal(char *buf){ //binary 표현을 decimal로 바꿔서 리턴
	int result=0;
	int square=1;
	int tmp=0;
	for(int i=4;i>0;i--){ //1바이트씩 끊어서 저장
		for(int j=0;j<i*2-1;j++)
			square*=16;
		result+=buf[tmp]/16*square;
		result+=buf[tmp++]%16*square/16;
		square=1;
	}
	if(result<0)
		return -1;
	return result;
}
