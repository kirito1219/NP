#include <iostream>
#include <string>
#include <cstring>
#include <vector>
#include <sstream>
#include <algorithm>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <map>
#include <stdlib.h>
#include<unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define True (1)
#define WRITE_END (1)
#define READ_END (0)
using namespace std;
int head=0,handle_ptr=0;

struct pipeNode{
	int fd_in_pipe[2];
	int count;
};

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

void childHandler(int signo){
  int status;
  while (waitpid(-1, &status, WNOHANG) > 0);

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



int main(int argc,char *argv[]){
	int fd[2];
	int err_mark=0;
	string token;
	int reserve;					// reserve main project std_in
	int build_in=0;
	pid_t pid,last_process_fork,pid_for_server;
	int jumpcount = 0 ;
	int need_new_pipe=1;
	string str,tmpstr; 			//parsing
	setenv("PATH","bin:.", 1); 	//initial path
	vector<string> res;
	vector<string> tmp_res;
	vector<string> tmp2_res;
	vector<string> piperes;	
	pipeNode pipe_node_data;
	vector<pipeNode> Pipenode;	
	
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

	while(True){
		forClientSockfd = accept(sockfd,(struct sockaddr*) &clientINFO, (socklen_t*)&addrlen);
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
			
			while(True){
				cout << "% ";
				getline(cin,str);
				//str.replace(str.find("\r"),2,"");
				istringstream delim(str);
				res.clear();					
				while(getline(delim,token,' ')){
					res.push_back(token);			
				}
				while(handle_ptr < res.size()){	
						signal(SIGCHLD, childHandler);
						if(!res[handle_ptr].find("|")||!res[handle_ptr].find("!")||!res[handle_ptr].find(">")||(handle_ptr+1)==res.size()){
							if(!res[handle_ptr].find("|")||!res[handle_ptr].find("!")){						
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
								
								tmp_res.clear();						
								for(int j=head;j<handle_ptr;j++){
									tmp_res.push_back(res[j]);
									head++;                            
								}
								head++;
													
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
								int write_port;					
								
								tmp_res.clear();						
								for(int j=head;j<handle_ptr;j++){
									tmp_res.push_back(res[j]);
									head++;                            
								}
								head++;	
								
								char *file_content[tmp_res.size()+1];
								Vector_to_Char(file_content,tmp_res);
								
								tmp_res.clear();						
								for(int j=head;j < res.size();j++){
										tmp_res.push_back(res[j]);
										head++;					
								}
													
								handle_ptr = res.size();	
								string file_name = tmp_res[0];										
								write_port = open(file_name.c_str(),O_WRONLY|O_CREAT|O_TRUNC,00777);						
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
								if(Pipenode.size()>0){						//若有等待的pipe， count--
									Pipe_Decrease(Pipenode);
								}
							}
							else if(res[head]=="printenv" || res[head]=="setenv" || res[head] =="exit"){	
								if(res[head]=="printenv"){
									head++;
									if(head!=res.size()){
										if(getenv(res[head].c_str())!=NULL){
											cout << getenv(res[head].c_str()) << endl;
										}
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
									exit(1);
								}
								if(Pipenode.size()>0){						//若有等待的pipe， count--
									Pipe_Decrease(Pipenode);
								
								}
							}						
							else if((handle_ptr+1)==res.size()){						
								tmp_res.clear();							
								for(int j=head;j < res.size();j++){
										tmp_res.push_back(res[j]);
										head++;					
								}													
								char *b[tmp_res.size()+1];
								Vector_to_Char(b,tmp_res);
								int pipe_to_node = find_zero_pipe_node(Pipenode);
								
								if(Pipenode.size()>0&&pipe_to_node>0){
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
						err_mark=0;
						handle_ptr++;	
				}
				
				handle_ptr=0;
				head=0;
			}
		
		}	
		else{		
		close(forClientSockfd);	
		}		
	}	

	
}
	



