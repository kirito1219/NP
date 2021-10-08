#include <iostream>
#include <stdio.h>
#include <string>
#include <cstring>
#include <vector>
#include <sstream>
#include <algorithm>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <map>
#include <stdlib.h>
#include<unistd.h>
//--socket
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
//--shm
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <signal.h>
//--
#define True (1)
#define WRITE_END (1)
#define READ_END (0)
using namespace std;
int head=0,handle_ptr=0;
char *write_buffer;
int shm_id_msg;
int shm_id_data;
int shm_id_pipe;
int own_pipe[31]={0};

struct pipeNode{
	int fd_in_pipe[2];
	int count;
};

struct user_inf{
	int user_pid;
	int user_id;
	char user_name[20];
	char user_ip[INET_ADDRSTRLEN];
	char user_port[20];	
}*user_data;

struct user_pipe_memory{
	int fifo_fd[31];
	int received[31];
}*user_pipe;

int find_zero_pipe_node(vector<pipeNode>Pipenode){
	for(int i=0;i<Pipenode.size();i++){
		if(Pipenode[i].count==0){
			return i+1;
		}
	}
	return 0;
}


int same_count_pipe_node(vector<pipeNode>Pipenode,int jumpcount){
	for(int i=0;i<Pipenode.size();i++){
		if(Pipenode[i].count==jumpcount){
			return i+1;
		}
	}
	return 0;
}

void Vector_to_Char( char *s[],const vector<string> vect){
	int k=1;
	for(int i=0;i<vect.size();i++){
		s[i] = (char*)vect[i].c_str();
		k=i;
	}
	s[k+1]=NULL;
}

pid_t fork_amount_check(pid_t pid){
	pid_t i;
	while((i=fork())<0){
		usleep(500);
	}
	return i;
}

void signalHandler(int signo){
	switch (signo){
		case SIGCHLD:
			int status;
			while (waitpid(-1, &status, WNOHANG) > 0);
			break;
		case SIGUSR1:
			cout << write_buffer << endl;	
			break;
		case SIGUSR2:
			char fifo_name[20];
			int fifo_out;
			for(int i=1;i<31;i++){
				for(int k=1;k<31;k++){
					if(user_pipe[i].received[k]==1){
						sprintf(fifo_name,"%dto%d",i,k);										
						fifo_out = open(fifo_name,O_RDONLY);
						user_pipe[i].fifo_fd[k]=fifo_out;
						user_pipe[i].received[k]=0;
						break;
					}
				}
			}
			break;
		case SIGINT:
			shmdt(user_data);
			shmctl(shm_id_data,IPC_RMID,0); 		
			shmdt(write_buffer);
			shmctl(shm_id_msg,IPC_RMID,0); 
			shmdt(user_pipe);
			shmctl(shm_id_pipe,IPC_RMID,0); 
			exit(1);
			break;
	}
}

void Pipe_Decrease(vector<pipeNode> &Pipenode){
	for(int i=0;i<Pipenode.size();i++){
		Pipenode[i].count--;
	}
	for(int i=0;i<Pipenode.size();i++){
		if(Pipenode[i].count==(-1)){
			Pipenode.erase(Pipenode.begin()+i);
		}
	}	
}

int find_name(user_inf *user_data,string name){

	for(int i=1;i<31;i++){
		if(user_data[i].user_name==name){
			return i;
		}
	}				
	return 0;
}

int find_id(user_inf *user_data,int id){

	for(int i=1;i<31;i++){
		if(user_data[i].user_id==id){
			return i;
		}
	}		
	return 0;
}


void Broadcast_user(user_inf *user_data,char* write_buffer,string logmsg){
	int count=1;
	memset(write_buffer,'\0',sizeof(write_buffer));
	strcpy(write_buffer,logmsg.c_str());	
	/*while(user_data[count].user_id>0){
		kill(user_data[count].user_pid,SIGUSR1);
		count++;
	}*/
	for(int i=1;i<31;i++){
		if(user_data[i].user_pid>0){
			kill(user_data[i].user_pid,SIGUSR1);
		}
	}
}

int user_empty(user_inf *user_data){
	int find=0,i=1;
	while(find==0){
		if(user_data[i].user_id==0){
			find = 1;
		}
		i++;
	}
	return i-1;
}



int main(int argc,char *argv[]){
	int fd[2];
	int err_mark=0;
	string token;
	int reserve;					// reserve main project std_in
	int build_in=0;
	pid_t pid,last_process_fork,pid_for_server;
	int jumpcount = 0 ;
	int need_new_pipe=1;
	int set_input_pipe = 0;
	int input_error=0;
	int send_user;
	int user_pipe_aim = 0;
	int user_pipe_from = 0;	
	string str,tmpstr,logmsg; 			//parsing
	setenv("PATH","bin:.", 1); 	//initial path
	vector<string> res;
	vector<string> tmp_res;
	vector<string> tmp2_res;
	vector<string> piperes;	
	pipeNode pipe_node_data;
	vector<pipeNode> Pipenode;	
	int user_list[31]={0};
	int read_error = open("/dev/null",O_RDWR);
	char ip_str[INET_ADDRSTRLEN];   //僅為ip指標
	char port_str[INET_ADDRSTRLEN];   //僅為port指標	
	int fifo_in;
	int fifo_out;
	string welcommsg = "****************************************\n** Welcome to the information server. **\n****************************************";
	
	signal(SIGCHLD, signalHandler);
	signal(SIGUSR1, signalHandler);
	signal(SIGUSR2, signalHandler);
	signal(SIGINT, signalHandler);
	
    //socket的建立
    int sockfd = 0, forClientSockfd = 0;
    sockfd = socket(AF_INET , SOCK_STREAM , 0);
    if (sockfd == -1){
        cout<<"Fail to create a socket."<<endl;
    }	
    //socket的連線
    struct sockaddr_in serverINFO,clientINFO;
    int addrlen = sizeof(clientINFO);
    bzero(&serverINFO,sizeof(serverINFO));	
	
	int flag=1,len=sizeof(int);
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &flag, len);
	
    serverINFO.sin_family = PF_INET;
    serverINFO.sin_addr.s_addr = INADDR_ANY;
    serverINFO.sin_port = htons(atoi(argv[1]));
    bind(sockfd,(struct sockaddr *)&serverINFO,sizeof(serverINFO));
    listen(sockfd,30);

	//--shm			
	shm_id_data = shmget((key_t)(atoi(argv[1])), 31 * sizeof(struct user_inf), 0644|IPC_CREAT);
	shm_id_msg = shmget((key_t)(atoi(argv[1])+1), sizeof(write_buffer), 0644|IPC_CREAT);
	shm_id_pipe = shmget((key_t)(atoi(argv[1])+2), 31 * sizeof(struct user_pipe_memory), 0644|IPC_CREAT);
	
	user_data = (user_inf*)shmat(shm_id_data,0,0);
	write_buffer = (char*)shmat(shm_id_msg,0,0);
	user_pipe = (user_pipe_memory*)shmat(shm_id_pipe,0,0);
	
	memset(write_buffer,'\0',sizeof(write_buffer));	
	
	for(int h=1;h<31;h++){
		user_data[h].user_id = 0;
		user_data[h].user_pid = 0;
		memset(user_data[h].user_name,'\0',sizeof(user_data[h].user_name));	
		memset(user_data[h].user_ip,'\0',sizeof(user_data[h].user_ip));	
		memset(user_data[h].user_port,'\0',sizeof(user_data[h].user_port));	
	}	
		
	for(int i=1;i<31;i++) {
		for(int k=1;k<31;k++){
			user_pipe[i].fifo_fd[k] = 0;
			user_pipe[i].received[k] = 0;			
		}
	}


	
	while(True){
		forClientSockfd = accept(sockfd,(struct sockaddr*) &clientINFO, (socklen_t*)&addrlen);
		int id = (user_empty(user_data));				
		pid_for_server = fork();
		if(pid_for_server==0){	
			close(STDIN_FILENO);
			close(STDOUT_FILENO);
			close(STDERR_FILENO);
			close(sockfd);
			dup(forClientSockfd);
			dup(forClientSockfd);
			dup(forClientSockfd);
			close(forClientSockfd);	
			//--login msg
			cout << welcommsg << endl;
			logmsg = "*** User '"+string(user_data[id].user_name)+"' entered from "+user_data[id].user_ip+":"+user_data[id].user_port+". ***";
			Broadcast_user(user_data,write_buffer,logmsg);			
			while(True){
				write(STDOUT_FILENO,"% ",2);
				
				char tmp[15000];
				char buffer[15000];
				memset(buffer,'\0',sizeof(buffer));
				memset(tmp,'\0',sizeof(buffer));
				read(STDIN_FILENO,buffer,sizeof(buffer));
				for(int i=0;i<strlen(buffer);i++){
					if(buffer[i]=='\r' || buffer[i]=='\n'){
						continue;
					}
					tmp[i]=buffer[i];
				}
				
				str=tmp;
				
				memset(buffer,'\0',sizeof(buffer));
				memset(tmp,'\0',sizeof(buffer));			
				
				
				
				
				//getline(cin,str);
				//str.replace(str.find("\r"),1,"\0");
				istringstream delim(str);
				res.clear();					
				while(getline(delim,token,' ')){
						if(!token.find("<")){
							if(!res[res.size()-1].find(">")){
								res.insert(res.end()-1,token);
							}
							else{
								res.push_back(token);
							}
						}else{
						res.push_back(token);
						}
				}
				while(handle_ptr < res.size()){							
						if(!res[handle_ptr].find("|")||!res[handle_ptr].find("!")||!res[handle_ptr].find(">")||(handle_ptr+1)==res.size()){
							if(res[head]=="printenv" || res[head]=="setenv" || res[head] =="exit" || res[head] =="name" || res[head] == "who" || res[head]=="tell"||res[head]=="yell"){	
								if(res[head]=="printenv"){
									head++;
									if(head!=res.size()){
										cout << getenv(res[head].c_str()) << endl;
									}
								}
								else if(res[head]=="setenv"){		
									head++;
									string cmd = res[head];
									head++;
									tmpstr = res[head];
									setenv(cmd.c_str(),tmpstr.c_str(),1);	
								}					
								else if(res[head]=="exit"){
									logmsg = "*** User '"+ string(user_data[id].user_name) +"' left. ***"; 
									Broadcast_user(user_data,write_buffer,logmsg);										
									for(int i=1;i<31;i++){
										char fifo_name[20];
										close(user_pipe[id].fifo_fd[i]);
										sprintf(fifo_name,"%dto%d",id,i);																					
										unlink(fifo_name);
										user_pipe[id].received[i]=0;
										user_pipe[id].fifo_fd[i]=0;
										
										char fifo_name2[20];
										close(user_pipe[i].fifo_fd[id]);
										sprintf(fifo_name2,"%dto%d",i,id);																					
										unlink(fifo_name2);
										user_pipe[i].received[id]=0;
										user_pipe[i].fifo_fd[id]=0;
									}
									user_data[id].user_id=0;								
									exit(1);
								}
								else if(res[head] =="yell"){
									head++;
									logmsg="*** "+string(user_data[id].user_name)+" yelled ***: ";
									while(head < res.size()){
										logmsg += res[head]+" ";
										head++;   
									}																											
									handle_ptr = head;
									Broadcast_user(user_data,write_buffer,logmsg);	
								}
								else if(res[head]=="who"){
									cout << "<ID>" <<"\t" << "<nickname>" << "\t" << "<IP:port>" <<"\t"<<"<indicate me>" <<endl;
									int who_count;
									for(who_count=1;who_count<31;who_count++){	
										if(user_data[who_count].user_id!=0){
											if (who_count == id) {
												cout << user_data[who_count].user_id <<"\t" << user_data[who_count].user_name <<"\t" << user_data[who_count].user_ip<<":" <<user_data[who_count].user_port<<"\t" <<"<-me" <<endl;
											}
											else if(who_count != id){
												cout << user_data[who_count].user_id << "\t" << user_data[who_count].user_name <<"\t" << user_data[who_count].user_ip<<":" <<user_data[who_count].user_port<<endl;
											}
										}
										
									}										
								}
								else if(res[head]=="name"){
									head++;										
									int name_exist=find_name(user_data,res[head]);
									if(name_exist==0){
										strcpy(user_data[id].user_name,res[head].c_str());
										logmsg="*** User from "+string(user_data[id].user_ip)+":"+user_data[id].user_port+" is named '"+res[head]+"'. ***";
										Broadcast_user(user_data,write_buffer,logmsg);	
									}
									else{
										cout << "*** User '"+res[head]+"' already exists. ***" <<endl;
									}
								}
								else if(res[head] =="tell"){
									
									head++;
									string user_target;
									stringstream string_to_int;
									user_target = res[head];
									string_to_int << user_target;
									string_to_int >> send_user;									
									head++;
									logmsg.clear();
									int id_exist=find_id(user_data,send_user);
									while(head < res.size()){
										logmsg += res[head]+" ";
										head++;   
									}								
									if(id_exist>0){
										logmsg="*** "+string(user_data[id].user_name)+" told you ***: "+logmsg;
										memset(write_buffer,'\0',sizeof(write_buffer));
										strcpy(write_buffer,logmsg.c_str());
										kill(user_data[id_exist].user_pid,SIGUSR1);
									}
									else{
										logmsg="*** Error: user #"+user_target+" does not exist yet. ***";
										memset(write_buffer,'\0',sizeof(write_buffer));
										strcpy(write_buffer,logmsg.c_str());
										kill(user_data[id].user_pid,SIGUSR1);
									}
									
									handle_ptr = head;																				
								}
								if(Pipenode.size()>0){						//若有等待的pipe， count--
									Pipe_Decrease(Pipenode);								
								}
							}							
							else if(!res[handle_ptr].find("|")||!res[handle_ptr].find("!")){						
								char *countstr;
								string pipecount;
								pipecount = res[handle_ptr];						
								pipecount = pipecount.assign(pipecount, 1, 4);
								jumpcount = atoi(pipecount.c_str()) ;            	//將|後的數字提出
								
								need_new_pipe=1;
								
								if(!res[handle_ptr].find("!")){
									err_mark=1;
								}
													
								if(jumpcount==0){
									jumpcount = 1 ;
								}
								
								string pipe_from;
								string user_data_id_string;	
								tmp_res.clear();						
								for(int j=head;j<handle_ptr;j++){										
									if(!res[j].find("<")){
										set_input_pipe = 1;
										pipe_from = res[j];
										pipe_from = pipe_from.assign(pipe_from, 1, 4);
										user_pipe_from = atoi(pipe_from.c_str());																		
										stringstream int_to_string;
										int_to_string << user_data[id].user_id;
										int_to_string >> user_data_id_string;																						
									}
									else{
										tmp_res.push_back(res[j]);
									}
									head++;                            
								}
								head++;																		
								if(set_input_pipe==1){
									if(user_data[user_pipe_from].user_id<=0){
										cout << "*** Error: user #"+pipe_from+" does not exist yet. ***"<<endl;				
										input_error = 1;
									}
									else if (user_pipe[user_pipe_from].fifo_fd[id]==0){
										cout << "*** Error: the pipe #"+pipe_from+"->#"+user_data_id_string+" does not exist yet. ***" << endl;									
										input_error = 1;
									}
									else{
										logmsg="*** "+string(user_data[id].user_name)+" (#"+user_data_id_string+") just received from "+user_data[user_pipe_from].user_name+" (#"+pipe_from+") by '"+str+"' ***";
										Broadcast_user(user_data,write_buffer,logmsg);
									}
									if(input_error==1){
										while(pipe(fd)<0){
											usleep(500);
										}
										pipe_node_data.fd_in_pipe[READ_END] = fd[READ_END];
										pipe_node_data.fd_in_pipe[WRITE_END] = fd[WRITE_END];
										dup2(read_error,fd[READ_END]);																			
									}else{
										pipe_node_data.fd_in_pipe[READ_END] = user_pipe[user_pipe_from].fifo_fd[id];
										pipe_node_data.fd_in_pipe[WRITE_END] = -1;
									}
									pipe_node_data.count = 0;
									Pipenode.push_back(pipe_node_data);
									user_pipe[user_pipe_from].fifo_fd[id] = 0;
								}
													
								char *a[tmp_res.size()+1];	
								Vector_to_Char(a,tmp_res);						
								int pip_size = Pipenode.size();
							
								if(pip_size>0){													
									int same_count = same_count_pipe_node(Pipenode,jumpcount);	//找到跟目的地一樣的pipe將資料放進去	
									if(same_count>0){
										same_count = same_count -1;
										need_new_pipe=0;											//回傳避免0所以+1 扣回來
										if(int find_zero = find_zero_pipe_node(Pipenode)){		//有一樣目的地且有pipe要傳入
											find_zero = find_zero-1;								//回傳避免0所以+1 扣回來
											close(Pipenode[find_zero].fd_in_pipe[WRITE_END]);
											pid=fork_amount_check(pid);
											if(pid==0){									
												dup2(Pipenode[find_zero].fd_in_pipe[READ_END],STDIN_FILENO);
												close(Pipenode[find_zero].fd_in_pipe[READ_END]);
												//---------------------------------------------
												close(Pipenode[same_count].fd_in_pipe[READ_END]);
												if(err_mark==1)dup2(Pipenode[same_count].fd_in_pipe[WRITE_END],STDERR_FILENO);
												dup2(Pipenode[same_count].fd_in_pipe[WRITE_END],STDOUT_FILENO);
												close(Pipenode[same_count].fd_in_pipe[WRITE_END]);
												if(execvp(a[0],a)<0){
													cerr << "Unknown command: ["<<a[0]<<"]."<<endl;
													exit(1);
												}
											}
											else{
												close(Pipenode[find_zero].fd_in_pipe[READ_END]);
											}
										}
										else{
											pid=fork_amount_check(pid);
											if(pid==0){
												close(Pipenode[same_count].fd_in_pipe[READ_END]);
												if(err_mark==1)dup2(Pipenode[same_count].fd_in_pipe[WRITE_END],STDERR_FILENO);
												dup2(Pipenode[same_count].fd_in_pipe[WRITE_END],STDOUT_FILENO);
												close(Pipenode[same_count].fd_in_pipe[WRITE_END]);
												if(execvp(a[0],a)<0){
													cerr << "Unknown command: ["<<a[0]<<"]."<<endl;
													exit(1);
												}
											}
										}
									}
									else{								
										if(int find_zero = find_zero_pipe_node(Pipenode)){		//有pipe要傳入處理	
											need_new_pipe=0;
											while(pipe(fd)<0){
												usleep(500);
											}
											find_zero = find_zero-1;	
											close(Pipenode[find_zero].fd_in_pipe[WRITE_END]);
											pid=fork_amount_check(pid);
											if(pid==0){										
												dup2(Pipenode[find_zero].fd_in_pipe[READ_END],STDIN_FILENO);
												close(Pipenode[find_zero].fd_in_pipe[READ_END]);
												//---------------------------------------------
												close(fd[READ_END]);
												if(err_mark==1)dup2(fd[WRITE_END],STDERR_FILENO);
												dup2(fd[WRITE_END],STDOUT_FILENO);
												close(fd[WRITE_END]);
												if(execvp(a[0],a)<0){
													cerr << "Unknown command: ["<<a[0]<<"]."<<endl;
													exit(1);
												}
											}
											else{
												close(Pipenode[find_zero].fd_in_pipe[READ_END]);
												pipe_node_data.fd_in_pipe[READ_END] = fd[READ_END];
												pipe_node_data.fd_in_pipe[WRITE_END] = fd[WRITE_END];
												pipe_node_data.count = jumpcount;
												Pipenode.push_back(pipe_node_data);										
											}
										}
									}																																								
								}
								if(need_new_pipe==1){												//無目的地一樣的額外開pipe處理							
									while(pipe(fd)<0){
										usleep(500);
									}
									pid=fork_amount_check(pid);																														
									if (pid==0){	
										close(fd[READ_END]);
										if(err_mark==1)dup2(fd[WRITE_END],STDERR_FILENO);
										dup2(fd[WRITE_END], STDOUT_FILENO); 
										close(fd[WRITE_END]);
										if(execvp(a[0],a)<0){
											cerr << "Unknown command: ["<<a[0]<<"]."<<endl;
											exit(1);
										}

									}
									else{	
									pipe_node_data.fd_in_pipe[READ_END] = fd[READ_END];
									pipe_node_data.fd_in_pipe[WRITE_END] = fd[WRITE_END];
									pipe_node_data.count = jumpcount;
									Pipenode.push_back(pipe_node_data);												
									}
								}						
								if(Pipenode.size()>0){												//若有等待的pipe， count--
									Pipe_Decrease(Pipenode);
								}							
								
							}					
							else if (!res[handle_ptr].find(">")){	
							//-----------------------------------
								tmp_res.clear();
								string pipe_from;
								for(int j=head;j<handle_ptr;j++){										
									if(!res[j].find("<")){
										set_input_pipe = 1;
										pipe_from = res[j];
										pipe_from = pipe_from.assign(pipe_from, 1, 4);
										user_pipe_from = atoi(pipe_from.c_str());																															
									}
									else{
										tmp_res.push_back(res[j]);
									}
									head++;                            
								}
								head++;	
								
								char *file_content[tmp_res.size()+1];
								Vector_to_Char(file_content,tmp_res);																											
							//--------------------------------------	
								string pipe_aim;
								pipe_aim = res[handle_ptr];
								pipe_aim = pipe_aim.assign(pipe_aim, 1, 4);
								user_pipe_aim = atoi(pipe_aim.c_str());									
								string user_data_id_string;									
								stringstream int_to_string;
								int_to_string << user_data[id].user_id;
								int_to_string >> user_data_id_string;	
							//--------------------------------------
								if(pipe_aim.size()!='\0'){

										if(set_input_pipe==1){
											if(user_data[user_pipe_from].user_id<=0){
												cout << "*** Error: user #"+pipe_from+" does not exist yet. ***"<<endl;				
												input_error = 1;
											}
											else if (user_pipe[user_pipe_from].fifo_fd[id]==0){
												cout << "*** Error: the pipe #"+pipe_from+"->#"+user_data_id_string+" does not exist yet. ***" << endl;									
												input_error = 1;
											}
											else{
												logmsg="*** "+string(user_data[id].user_name)+" (#"+user_data_id_string+") just received from "+user_data[user_pipe_from].user_name+" (#"+pipe_from+") by '"+str+"' ***";
												Broadcast_user(user_data,write_buffer,logmsg);
											}
											if(input_error==1){
												while(pipe(fd)<0){
													usleep(500);
												}
												pipe_node_data.fd_in_pipe[READ_END] = fd[READ_END];
												pipe_node_data.fd_in_pipe[WRITE_END] = fd[WRITE_END];
												dup2(read_error,fd[READ_END]);																			
											}else{
												pipe_node_data.fd_in_pipe[READ_END] = user_pipe[user_pipe_from].fifo_fd[id];
												pipe_node_data.fd_in_pipe[WRITE_END] = -1;
											}
											pipe_node_data.count = 0;
											Pipenode.push_back(pipe_node_data);
											user_pipe[user_pipe_from].fifo_fd[id] = 0;
										}
									
									if(user_pipe[id].fifo_fd[user_pipe_aim]>0){	//1=full 0= empty
										cout << "*** Error: the pipe #"+user_data_id_string+"->#"+pipe_aim+" already exists. ***" << endl;;										
									}
									else{
										handle_ptr = res.size();
								
										if(user_data[user_pipe_aim].user_id>0){
											int pipe_to_node = find_zero_pipe_node(Pipenode);												
											if(pipe_to_node>0){
												pipe_to_node = pipe_to_node-1;
												char fifo_name[20];
												sprintf(fifo_name,"%dto%d",id,user_pipe_aim);
												user_pipe[id].received[user_pipe_aim] = 1;
												mkfifo(fifo_name, 0666);
												kill(user_data[user_pipe_aim].user_pid,SIGUSR2);											
												fifo_out = open(fifo_name,O_WRONLY);
												close(Pipenode[pipe_to_node].fd_in_pipe[WRITE_END]);
												last_process_fork=fork_amount_check(last_process_fork);																																										
												if (last_process_fork==0){
														dup2(Pipenode[pipe_to_node].fd_in_pipe[READ_END],STDIN_FILENO);
														close(Pipenode[pipe_to_node].fd_in_pipe[READ_END]);
													//---------------------------------------------------------
														if(err_mark==1)dup2(fifo_out,STDERR_FILENO);
														dup2(fifo_out,STDOUT_FILENO);
														close(fifo_out);	
														if(execvp(file_content[0],file_content)<0){
															cerr << "Unknown command: ["<<file_content[0]<<"]."<<endl;
															exit(1);
														}						
												}
												else{
													close(Pipenode[pipe_to_node].fd_in_pipe[READ_END]);
													close(fifo_out);
													unlink(fifo_name);						
												}							
											}
											else{
												char fifo_name[20];
												sprintf(fifo_name,"%dto%d",id,user_pipe_aim);
												user_pipe[id].received[user_pipe_aim] = 1;
												mkfifo(fifo_name, 0666);
												kill(user_data[user_pipe_aim].user_pid,SIGUSR2);																																													
												fifo_out = open(fifo_name,O_WRONLY);												
												last_process_fork=fork_amount_check(last_process_fork);																																												
												if (last_process_fork==0){
													dup2(fifo_out,STDOUT_FILENO);
													close(fifo_out);
													if(execvp(file_content[0],file_content)<0){
														cerr << "Unknown command: ["<<file_content[0]<<"]."<<endl;
														exit(1);
													}						
												}
												else{
													close(fifo_out);
													unlink(fifo_name);
												}
											}
											logmsg =  "*** "+string(user_data[id].user_name)+" (#"+user_data_id_string+") just piped '"+str+"' to "+user_data[user_pipe_aim].user_name+" (#"+pipe_aim+") ***";												
											Broadcast_user(user_data,write_buffer,logmsg);
										}
										else{
												cout<<"*** Error: user #"+pipe_aim+" does not exist yet. ***" << endl;
										}										
									}																	
								}
								else{
									int write_port;													
									tmp_res.clear();						
									for(int j=head;j < res.size();j++){
											tmp_res.push_back(res[j]);
											head++;					
									}
														
									handle_ptr = res.size();	
									string file_name = tmp_res[0];										
									write_port = open(file_name.c_str(),O_WRONLY|O_CREAT|O_TRUNC,00777);						
									
									if(set_input_pipe==1){
										if(user_data[user_pipe_from].user_id<=0){
											cout << "*** Error: user #"+pipe_from+" does not exist yet. ***"<<endl;				
											input_error = 1;
										}
										else if (user_pipe[user_pipe_from].fifo_fd[id]==0){
											cout << "*** Error: the pipe #"+pipe_from+"->#"+user_data_id_string+" does not exist yet. ***" << endl;									
											input_error = 1;
										}
										else{
											logmsg="*** "+string(user_data[id].user_name)+" (#"+user_data_id_string+") just received from "+user_data[user_pipe_from].user_name+" (#"+pipe_from+") by '"+str+"' ***";
											Broadcast_user(user_data,write_buffer,logmsg);
										}
										if(input_error==1){
											while(pipe(fd)<0){
												usleep(500);
											}
											pipe_node_data.fd_in_pipe[READ_END] = fd[READ_END];
											pipe_node_data.fd_in_pipe[WRITE_END] = fd[WRITE_END];
											dup2(read_error,fd[READ_END]);																			
										}else{
											pipe_node_data.fd_in_pipe[READ_END] = user_pipe[user_pipe_from].fifo_fd[id];
											pipe_node_data.fd_in_pipe[WRITE_END] = -1;
										}
										pipe_node_data.count = 0;
										Pipenode.push_back(pipe_node_data);
										user_pipe[user_pipe_from].fifo_fd[id] = 0;
									}									
									int pipe_to_node = find_zero_pipe_node(Pipenode);
									if(pipe_to_node>0){
										pipe_to_node = pipe_to_node-1;								
										close(Pipenode[pipe_to_node].fd_in_pipe[WRITE_END]);
										last_process_fork=fork_amount_check(last_process_fork);																																										
										if (last_process_fork==0){
												dup2(Pipenode[pipe_to_node].fd_in_pipe[READ_END],STDIN_FILENO);
												close(Pipenode[pipe_to_node].fd_in_pipe[READ_END]);	
												dup2(write_port,STDOUT_FILENO);
												if(execvp(file_content[0],file_content)<0){
													cerr << "Unknown command: ["<<file_content[0]<<"]."<<endl;
													exit(1);
												}						
											}
										else{
											close(Pipenode[pipe_to_node].fd_in_pipe[READ_END]);
											int status;
											waitpid(last_process_fork, &status, 0);							
										}
										if(Pipenode.size()>0){						//若有等待的pipe， count--
											Pipe_Decrease(Pipenode);
										}							
									}
									else{
										last_process_fork=fork_amount_check(last_process_fork);
										if (last_process_fork==0){
												dup2(write_port,STDOUT_FILENO);
												if(execvp(file_content[0],file_content)<0){
													cerr << "Unknown command: ["<<file_content[0]<<"]."<<endl;
													exit(1);
												}						
											}
										else{
											int status;
											waitpid(last_process_fork, &status, 0);
										}
									}
								}
								if(Pipenode.size()>0){						//若有等待的pipe， count--
										Pipe_Decrease(Pipenode);
								}
							}																					
							else if((handle_ptr+1)==res.size()){						
								tmp_res.clear();
								string pipe_from;
								string user_data_id_string;								
								for(int j=head;j<res.size();j++){										
										if(!res[j].find("<")){
											set_input_pipe = 1;
											pipe_from = res[j];
											pipe_from = pipe_from.assign(pipe_from, 1, 4);
											user_pipe_from = atoi(pipe_from.c_str());
											stringstream int_to_string;
											int_to_string << user_data[id].user_id;
											int_to_string >> user_data_id_string;											
										}
										else{
											tmp_res.push_back(res[j]);
										}
										head++;                            
								}													
								char *b[tmp_res.size()+1];
								Vector_to_Char(b,tmp_res);
								
								if(set_input_pipe==1){
										if(user_data[user_pipe_from].user_id<=0){
											cout << "*** Error: user #"+pipe_from+" does not exist yet. ***"<<endl;				
											input_error = 1;
										}
										else if (user_pipe[user_pipe_from].fifo_fd[id]==0){
											cout << "*** Error: the pipe #"+pipe_from+"->#"+user_data_id_string+" does not exist yet. ***" << endl;									
											input_error = 1;
										}
										else{
											logmsg="*** "+string(user_data[id].user_name)+" (#"+user_data_id_string+") just received from "+user_data[user_pipe_from].user_name+" (#"+pipe_from+") by '"+str+"' ***";
											Broadcast_user(user_data,write_buffer,logmsg);
										}
										if(input_error==1){
											while(pipe(fd)<0){
												usleep(500);
											}
											pipe_node_data.fd_in_pipe[READ_END] = fd[READ_END];
											pipe_node_data.fd_in_pipe[WRITE_END] = fd[WRITE_END];
											dup2(read_error,fd[READ_END]);																			
										}else{
											pipe_node_data.fd_in_pipe[READ_END] = user_pipe[user_pipe_from].fifo_fd[id];
											pipe_node_data.fd_in_pipe[WRITE_END] = -1;
										}
										pipe_node_data.count = 0;
										Pipenode.push_back(pipe_node_data);
										user_pipe[user_pipe_from].fifo_fd[id] = 0;
								}																
								int pipe_to_node = find_zero_pipe_node(Pipenode);																																																			
								if(pipe_to_node>0){
									pipe_to_node=pipe_to_node-1;
									close(Pipenode[pipe_to_node].fd_in_pipe[WRITE_END]);
									last_process_fork=fork_amount_check(last_process_fork);
									if (last_process_fork==0){
										dup2(Pipenode[pipe_to_node].fd_in_pipe[READ_END],STDIN_FILENO);											
										close(Pipenode[pipe_to_node].fd_in_pipe[READ_END]);
										if(execvp(b[0],b)<0){
											cerr << "Unknown command: ["<<b[0]<<"]."<<endl;
											exit(1);
										}										
									}
									else{
										close(Pipenode[pipe_to_node].fd_in_pipe[READ_END]);
										int status;
										waitpid(last_process_fork, &status, 0);
									}
								}
								else{
									last_process_fork=fork_amount_check(last_process_fork);
									if (last_process_fork==0){			
										if(execvp(b[0],b)<0){
											cerr << "Unknown command: ["<<b[0]<<"]."<<endl;
											exit(1);
										}									
									}
									else{
										int status;
										waitpid(last_process_fork, &status, 0);
									}
								}
								if(Pipenode.size()>0){						//若有等待的pipe， count--
									Pipe_Decrease(Pipenode);
								}						
								
							}
						
						}
						input_error=0;
						err_mark=0;
						set_input_pipe=0;
						handle_ptr++;	
				}
				
				handle_ptr=0;
				head=0;
			}
		
		}	
		else{
		close(forClientSockfd);
		stringstream port_str;
		string port_msg;
		user_data[id].user_pid = pid_for_server;
		user_data[id].user_id = id;		
		strcpy(user_data[id].user_name,"(no name)");
		strcpy(user_data[id].user_ip,inet_ntop(PF_INET, &(clientINFO.sin_addr), ip_str, INET_ADDRSTRLEN));		
		port_str<<htons(clientINFO.sin_port);
		port_str>>port_msg;
		strcpy(user_data[id].user_port,port_msg.c_str());				
		}		
	}		
}
	



