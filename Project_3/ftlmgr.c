#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "flash.h"

FILE *flashfp;	

int dd_read(int ppn,char *pagebuf);
int dd_write(int ppb,char *pagebuf);
int dd_erase(int pbn);
int is_empty(int num,int size);

int main(int argc, char *argv[])
{	
	int fsize,inputSize,blockSize;
	int i;
	char pagebuf[PAGE_SIZE]; //page 버퍼
	char *blockbuf; //block 버퍼

	if(argc<4){ //프로그램 실행에 필요한 최소 인자를 입력하지 않은 경우 에러처리
		fprintf(stderr,"input all information!\n");
		exit(1);
	}

	if(strlen(argv[1])!=1){ //옵션이 올바르게 입력되지 않은 경우
		fprintf(stderr,"wrong option!\n");
		exit(1);
	}

	if(argv[1][0]=='c'){ //옵션이 'c'인 경우에만 파일을 생성
		if((flashfp=fopen(argv[2],"w+"))==NULL){
			fprintf(stderr,"file create error\n");
			exit(1);
		}
	}
	else{ //나머지 옵션인 경우 파일을 open
		if((flashfp=fopen(argv[2],"r+"))==NULL){
			fprintf(stderr,"file open error\n");
			exit(1);
		}
	}

	switch(argv[1][0]){

		case 'c': //create
			if(argc!=4){ //create에 올바른 인자 개수가 입력되지 않은 경우
				fprintf(stderr,"input right information!\n");
				fclose(flashfp);
				exit(1);
			}

			if(atoi(argv[3])<0){ //block 수가 너무 작은 경우 에러처리
				fprintf(stderr,"block is too small!\n");
				exit(1);
			}

			blockbuf=(char*)malloc(sizeof(char)*BLOCK_SIZE);

			for(i=0;i<atoi(argv[3]);i++){ //입력된 block 수만큼 file에 block을 생성
				memset((void*)blockbuf,(char)0xFF,BLOCK_SIZE);
				fwrite((void*)blockbuf,BLOCK_SIZE,1,flashfp);
			}
			free(blockbuf);
			break;

		case 'w': //write
			fseek(flashfp,0,SEEK_END); //파일 크기 계산
			fsize=ftell(flashfp);

			if(argc!=6){ //write에 올바른 인자 개수가 입력되지 않은 경우
				fprintf(stderr,"input right information!\n");
				exit(1);
			}

			if(atoi(argv[3])>=fsize/PAGE_SIZE || atoi(argv[3])<0){ //ppn이 너무 크거나 작은 경우 에러처리
				fprintf(stderr,"invalid ppn!\n");
				fclose(flashfp);
				exit(1);
			}

			memset((void*)pagebuf,(char)0xFF,PAGE_SIZE); //pagebuf 초기화
			
			inputSize=strlen(argv[4]);
			if(inputSize>SECTOR_SIZE) //입력받은 sectordata의 크기가 512 바이트를 넘어가는 경우 512바이트만 저장
				inputSize=SECTOR_SIZE;
			strncpy(pagebuf,argv[4],inputSize); //pagebuf의 sector 부분에 입력받은 sector 데이터 복사

			inputSize=strlen(argv[5]);
			if(inputSize>SPARE_SIZE) //입력받은 sparedata의 크기가 16 바이트를 넘어가는 경우 16바이트만 저장
				inputSize=SPARE_SIZE;
			strncpy(pagebuf+SECTOR_SIZE,argv[5],inputSize); //pagebuf의 spare 부분에 입력받은 spare 데이터 복사

			if(is_empty(atoi(argv[3]),PAGE_SIZE)){ //write할 page가 비어 있는 경우 write 진행
				if(dd_write(atoi(argv[3]),pagebuf)!=1){ //입력받은 data를 지정한 page에 write
					fprintf(stderr,"write error!\n");
					fclose(flashfp);
					exit(1);
				}
			}
			else{ //write할 page가 비어 있지 않은 경우 write before erase, write

				blockbuf=(char*)malloc(sizeof(char)*BLOCK_SIZE);
				fseek(flashfp,(atoi(argv[3])/4)*BLOCK_SIZE,SEEK_SET);
				fread(blockbuf,BLOCK_SIZE,1,flashfp); //write할 page의 block을 blockbuf에 복사
				memset((void*)(blockbuf+(atoi(argv[3])%PAGE_NUM)*PAGE_SIZE),(char)0xFF,PAGE_SIZE); //blockbuf에서 write할 page 초기화
				
				for(i=0;i<fsize/BLOCK_SIZE;i++){ //전체에서 비어있는 block을 처음부터 탐색
					
					if(is_empty(i,BLOCK_SIZE)){ //비어있는 block을 찾은 경우
						fseek(flashfp,i*BLOCK_SIZE,SEEK_SET); //비어있는 block에 blockbuf를 붙여넣음
						fwrite(blockbuf,BLOCK_SIZE,1,flashfp);

						if(dd_erase(atoi(argv[3])/PAGE_NUM)!=1){ //원래 block erase
							fprintf(stderr,"erase error!\n");
							exit(1);
						}

						fseek(flashfp,atoi(argv[3])/PAGE_NUM*BLOCK_SIZE,SEEK_SET); //원래 block으로 오프셋 이동
						fwrite(blockbuf,BLOCK_SIZE,1,flashfp); //blockbuf를 원래 block에 recopy

						if(dd_write(atoi(argv[3]),pagebuf)!=1){ //입력받은 sector,spare 데이터를 지정한 page에 write
							fprintf(stderr,"write error!\n");
							fclose(flashfp);
							exit(1);
						}
						if(dd_erase(i)!=1){ //복사한 block를 erase
							fprintf(stderr,"erase error!\n");
							exit(1);
						}
						break;
					}
				}
				free(blockbuf); 
			}
			break;

		case 'r': //read
			fseek(flashfp,0,SEEK_END);//파일 크기 계산
			fsize=ftell(flashfp);

			if(argc!=4){ //read에 올바른 인자 개수가 입력되지 않은 경우
				fprintf(stderr,"input right information!\n");
				exit(1);
			}

			if(atoi(argv[3])>=fsize/PAGE_SIZE || atoi(argv[3])<0){ //ppn이 너무 크거나 작은 경우 에러처리
				fprintf(stderr,"invalid ppn!\n");
				fclose(flashfp);
				exit(1);
			}

			if(is_empty(atoi(argv[3]),PAGE_SIZE)==1) //read할 page가 비어있으면 print하지 않음
				break;

			if(dd_read(atoi(argv[3]),pagebuf)!=1){ //pagebuf에 ppn에 해당되는 데이터 복사
				fprintf(stderr,"read error!\n");
				fclose(flashfp);
				exit(1);
			}

			for(i=0;i<SECTOR_SIZE;i++){ //해당 page의 sector data 출력
				if( pagebuf[i]!=(char)0xFF )
					printf("%c",pagebuf[i]);
				else
					break;
			}

			printf(" ");

			for(i=0;i<SPARE_SIZE;i++){ //해당 page의 spare data 출력
				if( pagebuf[SECTOR_SIZE+i]!=(char)0xFF )
					printf("%c",pagebuf[SECTOR_SIZE+i]);
				else
					break;
			}
			printf("\n");
			break;

		case 'e': //erase
			fseek(flashfp,0,SEEK_END);
			blockSize=ftell(flashfp)/BLOCK_SIZE;

			if(argc!=4){ //erase에 올바른 인자 개수가 입력되지 않은 경우
				fprintf(stderr,"input right information!\n");
				exit(1);
			}

			if(atoi(argv[3])>=blockSize || atoi(argv[3])<0){ //pbn이 너무 크거나 작은 경우 에러처리
				fprintf(stderr,"invalid pbn!\n");
				fclose(flashfp);
				exit(1);
			}

			if(dd_erase(atoi(argv[3]))!=1){ //pbn의 데이터를 삭제
				fprintf(stderr,"erase error!\n");
				fclose(flashfp);
				exit(1);
			}

			break;

		default: //c w r e 옵션이 아닌 다른 옵션이 입력된 경우 에러처리
			fprintf(stderr,"wrong option!\n");
			fclose(flashfp);
			exit(1);
	}

	fclose(flashfp);
	exit(0);
}

int is_empty(int num,int size){ //파일의 지정한 block 또는 page가 비어있는지 확인하는 함수
	fseek(flashfp,num*size,SEEK_SET);
	for(int i=0;i<size;i++){
		if(fgetc(flashfp)!=0xFF)
			return 0;
	}
	return 1;
}
