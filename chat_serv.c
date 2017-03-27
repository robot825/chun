#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

#define MAXLINE  511
#define MAX_SOCK 1024 // 솔라리스의 경우 64

char *EXIT_STRING = "exit";  
char *START_STRING = "▩▩▩▩▩ ▩▩▩▩▩ ▩▩▩▩▩ ▩▩▩▩▩ ▩▩▩▩▩\n▩▩▩▩▩ Welcome to Linux Chatting World! ▩▩▩▩▩\n▩▩▩▩▩ ver 1.0           ▩▩▩▩▩\n▩▩▩▩▩ ▩▩▩▩▩ ▩▩▩▩▩ ▩▩▩▩▩ ▩▩▩▩▩\n";		
char *MAIN_MENU="\t서비스 번호를 선택하세요.\n 1.채팅방 개설 \n 2.채팅 참여\n 3.회원 보기\n\n\n<번호/명령(go,exit)> "; 
char *CREAT_ROOM="enter chatting room subject ";
    
int maxfdp1;                // 최대 소켓번호 +1
int num_chat = 0;          // 채팅 참가자 수
int clisock_list[MAX_SOCK]; // 채팅에 참가자 소켓번호 목록
int listen_sock,addrlen = sizeof(struct sockaddr_in);
int serv_port=2402;
int rooms_num=0;

FILE *chat_log;

struct chat_room{
	 		// room structure
        char room_name[MAXLINE];
        int user_cnt;	 // entry num
        int user_list[MAX_SOCK]; // entry name list
};

struct chat_room chat_rooms[MAXLINE];

// 새로운 채팅 참가자 처리
void addClient(int s, struct sockaddr_in *newcliaddr);

int getmax();               // 최대 소켓 번호 찾기

void removeClient(int s);    // 채팅 탈퇴 처리 함수

int tcp_listen(int host, int port, int backlog); // 소켓 생성 및 listen

void errquit(char *mesg) { perror(mesg); exit(1); }

void get_time(void);

void creat_room(int sock);

void out_room_list(int sock);

void sig_pipe(void){	//안정적인 동작위해 SIPPIPE 무시
	struct sigaction spipe_act;
	spipe_act.sa_handler = SIG_IGN;
	spipe_act.sa_flags = 0;
	sigemptyset (&spipe_act.sa_mask);
	sigaddset (&spipe_act.sa_mask,SIGPIPE);
	sigaction (SIGPIPE,&spipe_act,NULL);
}

//main
int main(int argc, char *argv[]) {

	struct sockaddr_in cliaddr;
	char buf[MAXLINE+1];
	int i, j, k, l, nbyte, accp_sock; 	
	int flag;   //client의 채팅 참여 여부 판단
	fd_set read_fds;  // 읽기를 감지할 fd_set 구조체

	if(argc != 1) {
		printf("사용법 :%s \n", argv[0]);
		exit(0);
	}
	sig_pipe();	//SIG_PIPE 무시, 안정적인 동작위해
	if((chat_log=fopen("chat_serv.log","a+"))==NULL)
		errquit("loging failed!!");

	get_time(); //서버프로그램 시작 시간을 log파일에 저장
	fprintf(chat_log,"%s \t \n","Chatting Server Start");
	fflush(chat_log);

	listen_sock = tcp_listen(INADDR_ANY, serv_port,5);

	while(1) {
		FD_ZERO(&read_fds);
		FD_SET(listen_sock, &read_fds);

		for(i=0; i<num_chat; i++)
			FD_SET(clisock_list[i], &read_fds);

		maxfdp1 = getmax() + 1;     // maxfdp1 재 계산
		puts("wait for client");

		if(select(maxfdp1, &read_fds,NULL,NULL,NULL)<0)
			errquit("select fail");
		// 새로운 사용자가 연결 요청시
		if(FD_ISSET(listen_sock, &read_fds)) {

			accp_sock=accept(listen_sock, (struct sockaddr *)&cliaddr, &addrlen);

			if(accp_sock == -1)
				errquit("accept fail");

			addClient(accp_sock,&cliaddr);
			send(accp_sock, START_STRING, strlen(START_STRING), 0);
			send(accp_sock, MAIN_MENU, strlen(MAIN_MENU),0);	
			printf("%d번째 사용자 추가.\n", num_chat);
		}

        // 클라이언트가 보낸 메시지를 모든 클라이언트에게 방송
		for(i = 0; i < num_chat; i++) {

			if(FD_ISSET(clisock_list[i], &read_fds)) {
				nbyte = recv(clisock_list[i], buf, MAXLINE, 0);

                		if(nbyte<= 0) {
					removeClient(i);    // 클라이언트의 종료
					continue;
				}

				buf[nbyte] = 0;
				//채팅방에 참가한 client인 경우 
				if(flag==2){
					printf("flag ==2\n");
					if(strstr(buf,"0")!=NULL){
						chat_rooms[0].user_list[chat_rooms[0].user_cnt]=clisock_list[i];
						chat_rooms[0].user_cnt++;
						printf(" %d   %d \n",chat_rooms[0].user_cnt);
					}
				}	
				for(j=0,flag=0;j<rooms_num;j++){
					for(k=0;k<chat_rooms[j].user_cnt;k++){
						if(clisock_list[i]==chat_rooms[j].user_list[k]){
							flag=1;
							if(strstr(buf,EXIT_STRING) != NULL) {
								removeClient(i);    // 클라이언트의 종료
								break;
							}
							for (l = 0; l <chat_rooms[j].user_cnt; l++)
								send(chat_rooms[j].user_list[l], buf, nbyte, 0);
							break;
						}
					}
				}
				if(flag==1){ 
					printf("your are join the chatting room\n");
					continue;
				}
				
        // 종료문자 처리
				if(strstr(buf,EXIT_STRING) != NULL) {
					removeClient(i);    // 클라이언트의 종료
					continue;
				}
				
				else if(strstr(buf,"1")!=NULL){
					send(clisock_list[i],"방 제목을 입력하세요 :",sizeof("방 제목을 입력하세요 :"),0);
					creat_room(clisock_list[i]);
				}
				else if(strstr(buf,"2") != NULL) {
					out_room_list(clisock_list[i]);
					flag=2;	
				}

			} //end if
		} //end for
	}  // end of while
	return 0;
}

// 새로운 채팅 참가자 처리
void addClient(int s, struct sockaddr_in *newcliaddr) {
	char buf[20];
	inet_ntop(AF_INET,&newcliaddr->sin_addr,buf,sizeof(buf));
	get_time();
	fprintf(chat_log,"%-11s \t %s\n","connect",buf);
	fflush(chat_log);

     // 채팅 클라이언트 목록에 추가
	clisock_list[num_chat] = s;
	num_chat++;
}

// 채팅 탈퇴 처리
void removeClient(int s) {
	char buf[20];
	struct sockaddr_in del_cliaddr;

	// 탈퇴한 회원의 시간과 IP 주소 가져오기 
	getpeername(clisock_list[s],(struct sockaddr*)&del_cliaddr,&addrlen);

//	printf("%d",del_cliaddr.sin_addr);
	inet_ntop(AF_INET,&del_cliaddr.sin_addr,buf,sizeof(buf));
	get_time();
	fprintf(chat_log,"%-11s \t %s\n","disconnect",buf);
	fflush(chat_log);
	close(clisock_list[s]);
	
	if(s != num_chat-1)
		clisock_list[s] = clisock_list[num_chat-1];
	num_chat--;

	printf("채팅 참가자 1명 탈퇴. 현재 참가자 수 = %d\n", num_chat);
}

// 최대 소켓번호 찾기
int getmax() {
    // Minimum 소켓번호는 가정 먼저 생성된 listen_sock
	int max = listen_sock;
	int i;
	
	for (i=0; i < num_chat; i++)
		if (clisock_list[i] > max )
			max = clisock_list[i];
		return max;
}

// listen 소켓 생성 및 listen
int  tcp_listen(int host, int port, int backlog) {
	int sd;
	int opt_yes=1; 		// 소켓의 옵션값
	struct sockaddr_in servaddr;

	sd = socket(AF_INET, SOCK_STREAM, 0);
	
	if(sd == -1) {
		perror("socket fail");
		exit(1);
	}
	// 소켓의 옵션 변경 TIME-WAIT 시에도 재실행 가능
	if (setsockopt(sd, SOL_SOCKET,SO_REUSEADDR,&opt_yes,sizeof(opt_yes))<0)
		printf("error : reuse setsockopt\n");

	//SO_KEEPALIVE 지정 .상대방과 연결상태 점검
	if (setsockopt(sd, SOL_SOCKET,SO_KEEPALIVE,&opt_yes,sizeof(opt_yes))<0)
		printf("error ; keepalive \n");

    // servaddr 구조체의 내용 세팅
	bzero((char *)&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(host);
	servaddr.sin_port = htons(port);
	
	if (bind(sd , (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
		perror("bind fail");  exit(1);
	}
    // 클라이언트로부터 연결요청을 기다림
	listen(sd, backlog);
	return sd;
}
void get_time(){
	time_t get_time;
	struct tm *cur_time;
	
	time(&get_time);
	//cur_time=gmtime(&get_time);
	cur_time=localtime(&get_time);
	fprintf(chat_log,"%4d/%2d/%2d %2d:%2d:%2d \t",cur_time->tm_year+1900,cur_time->tm_mon+1,cur_time->tm_mday,
		cur_time->tm_hour,cur_time->tm_min,cur_time->tm_sec);
	fflush(chat_log);	
}

void creat_room(int sock){     //채팅방을 개설해 주는 함수
//	chat_rooms[rooms_num].user_cnt=0;	
	printf("creat room %d %d\n",sock,chat_rooms[rooms_num].user_cnt);
	while(1){
//		if((send(sock,CREAT_ROOM,sizeof(CREAT_ROOM),0))<0)
//			printf("failed\n");
		printf("succes send\n");
		if((recv(sock, chat_rooms[rooms_num].room_name, MAXLINE,0))<0)
			printf("failed recv\n");
		printf("succes recv\n");
		break;
	}
	chat_rooms[rooms_num].user_list[chat_rooms[rooms_num].user_cnt]=sock;
	printf("%s",chat_rooms[rooms_num].room_name);
	chat_rooms[rooms_num].user_cnt++;
	rooms_num++;
	printf("%d \n ",rooms_num);
	
}

void out_room_list(int sock){ //채팅방 목록을 보여주는 함수 
	int j;
	char room_list[10][MAXLINE];
	if(rooms_num<1)
		send(sock,"not exit room",sizeof("not exit room"),0);
	for(j=0;j<rooms_num;j++){
		sprintf(room_list[j],"%d. %s",j,chat_rooms[j].room_name);
	
	send(sock,room_list[j],sizeof(room_list[j]),0);	
	printf("%s\n",room_list[j]);
	}
}
