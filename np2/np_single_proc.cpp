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
#include <netdb.h>
#include <stdlib.h>
#include<unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include<iomanip>
#include <map>
#define True (1)
#define WRITE_END (1)
#define READ_END (0)
using namespace std;
int head=0,handle_ptr=0;

struct user_pipe_memory{
	int fd_user_pipe[2];
	int data_empty;
};

struct user_inf{
	int user_fd;
	string user_port;
	string user_name;
	string user_ip;
	int user_id;
	map<string, string> env;
};

struct pipeNode{
	int fd_in_pipe[2];
	int count;
	int ownerID;
};

int find_zero_pipe_node(vector<pipeNode>Pipenode,int user_id){
	for(int i=0;i<Pipenode.size();i++){
		if(Pipenode[i].count==0){
			if(Pipenode[i].ownerID==user_id){
				return i+1;
			}
		}
	}
	return 0;
}


int same_count_pipe_node(vector<pipeNode>Pipenode,int jumpcount,int user_id){
	for(int i=0;i<Pipenode.size();i++){
		if(Pipenode[i].count==jumpcount){
			if(Pipenode[i].ownerID==user_id){
				return i+1;
			}
		}
	}
	return 0;
}

void Vector_to_Char( char *s[],vector<string> vect){
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

void Pipe_Decrease(vector<pipeNode> &Pipenode,int user_id){
	for(int i=0;i<Pipenode.size();i++){
		if(Pipenode[i].ownerID==user_id){
			Pipenode[i].count--;
		}
	}
	for(int i=0;i<Pipenode.size();i++){
		if(Pipenode[i].count==(-1)){
			if(Pipenode[i].ownerID==user_id){
				Pipenode.erase(Pipenode.begin()+i);
			}
		}
	}	
}

void Broadcast_user(string msg,int broadcast_user,int socket_main,int fdmax,fd_set master,bool me){		//me為自己是否收到
	for(int count=0;count<=fdmax;count++){
		if (FD_ISSET(count, &master)) {
			if(me==0){
				if (count != socket_main && count != broadcast_user) {
					write(count, msg.c_str(), msg.size());
				}
			}
			else{
				if(count != socket_main){
					write(count, msg.c_str(), msg.size());
				}				
			}
		}		
	
	}
}

int find_id_fd(int find_id,int sockfd,int fdmax,fd_set master,user_inf user_data[]){
	for(int find_user=1;find_user<=fdmax;find_user++){
		if (FD_ISSET(find_user, &master)) {
			if (find_user != sockfd) {
				if(user_data[find_user].user_id==find_id){
					return find_user;
				}
			}
		}												
	}
	return -1;
}

int user_empty(int user_list[]){
	int find=0,i=1;
	while(find==0){
		if(user_list[i]==0){
			user_list[i] = 1;
			find = 1;
		}
		i++;
	}
	return i-1;
}


int main(int argc,char *argv[]){
	int fd[2];
	int err_mark=0;
	int input_error=0;
	int find_user=0;
	string token;
	int reserve;					// reserve main project std_in
	int build_in=0;
	pid_t pid,last_process_fork;
	int jumpcount = 0 ;
	int user_pipe_aim = 0;
	int user_pipe_from = 0;
	int need_new_pipe=1;
	int set_input_pipe = 0; //for "<" in "|" ">"
	int send_user,user_tmp,user_tmp_tmp;
	string str,tmpstr; 			//parsing
	string write_file_pars;		// for >
	string logmsg;
	setenv("PATH","bin:.", 1); 	//initial path
	vector<string> res;
	vector<string> tmp_res;
	vector<string> piperes;	
	pipeNode pipe_node_data;
	user_inf user_data[200];
	user_pipe_memory user_pipe[31][31];		//user_pipe
	int user_list[31]={0};
	map<string, string>::iterator iter;
	vector<pipeNode> Pipenode;
	int read_error = open("/dev/null",O_RDWR);
	char welcommsg[] = "****************************************\n** Welcome to the information server. **\n****************************************\n";
	int oldin = dup(STDIN_FILENO), oldout = dup(STDOUT_FILENO), olderr = dup(STDERR_FILENO);

    //socket的建立
    int sockfd = 0, forClientSockfd = 0,fdmax;
	fd_set master,read_fds;
    sockfd = socket(AF_INET , SOCK_STREAM , 0);

    if (sockfd == -1){
        cout<<"Fail to create a socket."<<endl;
    }
	
	FD_ZERO(&master);
	FD_ZERO(&read_fds);
    //socket的連線
    struct sockaddr_in serverINFO,clientINFO;
	char ip_str[INET_ADDRSTRLEN];   //僅為ip指標
	char port_str[INET_ADDRSTRLEN];   //僅為port指標
    int addrlen = sizeof(clientINFO);
    bzero(&serverINFO,sizeof(serverINFO));

	int flag=1,len=sizeof(int);
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &flag, len);
	
    serverINFO.sin_family = AF_INET;
    serverINFO.sin_addr.s_addr = INADDR_ANY;
    serverINFO.sin_port = htons(atoi(argv[1]));
    bind(sockfd,(struct sockaddr *)&serverINFO,sizeof(serverINFO));
    listen(sockfd,30);
	
	FD_SET(sockfd, &master);
	fdmax=sockfd;

		
	while(True){
		read_fds = master;
		while(select(fdmax+1, &read_fds, NULL, NULL, NULL)<0){
		}
		for(int select_count=0;select_count<=fdmax;select_count++){
			if (FD_ISSET(select_count, &read_fds)){
				if(select_count==sockfd){
					forClientSockfd = accept(sockfd,(struct sockaddr*) &clientINFO, (socklen_t*)&addrlen);
					FD_SET(forClientSockfd, &master); 
					if(forClientSockfd>fdmax){
						fdmax = forClientSockfd;
					}
					stringstream port_str;
					user_data[forClientSockfd].user_name = "(no name)";
					user_data[forClientSockfd].user_fd = forClientSockfd;
					user_data[forClientSockfd].user_ip = (char*)inet_ntop(AF_INET, &(clientINFO.sin_addr), ip_str, INET_ADDRSTRLEN);
					user_data[forClientSockfd].user_id = (user_empty(user_list));
					user_data[forClientSockfd].env["PATH"] = "bin:.";
					port_str<<htons(clientINFO.sin_port);
					port_str>>user_data[forClientSockfd].user_port;
					write(forClientSockfd, welcommsg, (sizeof welcommsg)-1);
					logmsg = "*** User '"+user_data[forClientSockfd].user_name+"' entered from "+user_data[forClientSockfd].user_ip+":"+user_data[forClientSockfd].user_port+". ***\n";
					Broadcast_user(logmsg,select_count,sockfd,fdmax,master,0);
					write(forClientSockfd,"% ",2);					
				}
				else{
					//---初始化ENV
					clearenv();
					for(iter = user_data[select_count].env.begin(); iter != user_data[select_count].env.end(); iter++){
						setenv((iter->first).c_str(),(iter->second).c_str(), 1);
					}//---------		
					dup2(user_data[select_count].user_fd,STDIN_FILENO);
					dup2(user_data[select_count].user_fd,STDOUT_FILENO);
					dup2(user_data[select_count].user_fd,STDERR_FILENO);
					
					char tmp[15000];
					char buffer[15000];
					memset(buffer,'\0',sizeof(buffer));
					memset(tmp,'\0',sizeof(buffer));
					read(user_data[select_count].user_fd,buffer,sizeof(buffer));
					for(int i=0;i<strlen(buffer);i++){
						if(buffer[i]=='\r' || buffer[i]=='\n'){
							continue;
						}
						tmp[i]=buffer[i];
					}
					
					str=tmp;
					
					memset(buffer,'\0',sizeof(buffer));
					memset(tmp,'\0',sizeof(buffer));
					
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
							signal(SIGCHLD, childHandler);
							if(!res[handle_ptr].find("|")||!res[handle_ptr].find("!")||!res[handle_ptr].find(">")||(handle_ptr+1)==res.size()){
								if(res[head]=="printenv" || res[head]=="setenv" || res[head] =="exit" || res[head] =="name" || res[head] =="who" || res[head] =="yell" || res[head] =="tell"){	
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
										user_data[select_count].env[cmd] = tmpstr;
										setenv(cmd.c_str(),tmpstr.c_str(),1);	
									}					
									else if(res[head]=="exit"){
										logmsg = "*** User '"+ user_data[select_count].user_name +"' left. ***\n"; 
										Broadcast_user(logmsg,select_count,sockfd,fdmax,master,1); 
										close(user_data[select_count].user_fd);
										dup2(oldin,STDIN_FILENO);
										dup2(oldout,STDOUT_FILENO);
										dup2(olderr,STDERR_FILENO);
										
										for(int i=1;i<31;i++){
											if(user_pipe[user_data[select_count].user_id][i].data_empty==1){
												close(user_pipe[user_data[select_count].user_id][i].fd_user_pipe[READ_END]);
												close(user_pipe[user_data[select_count].user_id][i].fd_user_pipe[WRITE_END]);
												user_pipe[user_data[select_count].user_id][i].data_empty=0;
											}
											if(user_pipe[i][user_data[select_count].user_id].data_empty==1){
												close(user_pipe[i][user_data[select_count].user_id].fd_user_pipe[READ_END]);
												close(user_pipe[i][user_data[select_count].user_id].fd_user_pipe[WRITE_END]);											
												user_pipe[i][user_data[select_count].user_id].data_empty=0;	
											}
										}
										
										user_list[user_data[select_count].user_id]=0;
										FD_CLR(user_data[select_count].user_fd, &master);
									}
									else if(res[head]=="name"){
										head++;
										string change_name = res[head];										
										int name_exist=0;										
										for(find_user=0;find_user<=fdmax;find_user++){
											if (FD_ISSET(find_user, &master)) {
												if (find_user != sockfd) {
													if(user_data[find_user].user_name==change_name){
														name_exist=1;
													}
												}
											}												
										}
										if(name_exist==0){
											user_data[select_count].user_name = change_name;
											logmsg="*** User from "+user_data[select_count].user_ip+":"+user_data[select_count].user_port+" is named '"+change_name+"'. ***\n";										
											Broadcast_user(logmsg,select_count,sockfd,fdmax,master,1);
										}
										else{
											cout << "*** User '"+change_name+"' already exists. ***" <<endl;
										}
									}
									else if(res[head]=="who"){
										cout << "<ID>" <<"\t" << "<nickname>" << "\t" << "<IP:port>" <<"\t" <<"<indicate me>" <<endl;																					
										for(find_user=0;find_user<=fdmax;find_user++){
											if (FD_ISSET(find_user, &master)) {
												if (find_user != sockfd && find_user==select_count) {
													cout << user_data[find_user].user_id <<"\t" << user_data[find_user].user_name <<"\t" << user_data[find_user].user_ip<<":" <<user_data[find_user].user_port<<"\t" <<"<-me" <<endl;
												}
												else if(find_user != sockfd){
													cout << user_data[find_user].user_id << "\t" << user_data[find_user].user_name <<"\t" << user_data[find_user].user_ip<<":" <<user_data[find_user].user_port<<endl;
												}
											}		
										}										
									}
									else if(res[head] =="yell"){
										head++;
										logmsg="*** "+user_data[select_count].user_name+" yelled ***: ";
										while(head < res.size()){
											logmsg += res[head]+" ";
											head++;   
										}
										logmsg += "\n";
										handle_ptr = head;
										Broadcast_user(logmsg,select_count,sockfd,fdmax,master,1);
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
										while(head < res.size()){
											logmsg += res[head]+" ";
											head++;   
										}
										logmsg += "\n";									
										if((user_tmp = find_id_fd(send_user,sockfd,fdmax,master,user_data))>0){
											logmsg="*** "+user_data[select_count].user_name+" told you ***: "+logmsg;
											write(user_tmp, logmsg.c_str(), logmsg.size());
										}
										else{
											logmsg="*** Error: user #"+user_target+" does not exist yet. ***\n";
											write(select_count, logmsg.c_str(), logmsg.size());
										}
										
										handle_ptr = head;
										
										
									}									
									if(Pipenode.size()>0){						//若有等待的pipe， count--
										Pipe_Decrease(Pipenode,user_data[select_count].user_id);
									
									}
								}
								else if(!res[handle_ptr].find("|")||!res[handle_ptr].find("!")){
									
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
									string pipe_aim;
									string user_data_id_string;	
									tmp_res.clear();						
									for(int j=head;j<handle_ptr;j++){										
										if(!res[j].find("<")){
											set_input_pipe = 1;
											pipe_aim = res[j];
											pipe_aim = pipe_aim.assign(pipe_aim, 1, 4);
											user_pipe_from = atoi(pipe_aim.c_str());																		
											stringstream int_to_string;
											int_to_string << user_data[select_count].user_id;
											int_to_string >> user_data_id_string;																						
										}
										else{
											tmp_res.push_back(res[j]);
										}
										head++;                            
									}
									head++;																		
									if(set_input_pipe==1){
										if((user_tmp_tmp = find_id_fd(user_pipe_from,sockfd,fdmax,master,user_data))<0){
											logmsg="*** Error: user #"+pipe_aim+" does not exist yet. ***\n";
											write(select_count, logmsg.c_str(), logmsg.size());
											input_error = 1;
										}
										else if (user_pipe[user_pipe_from][user_data[select_count].user_id].data_empty==0){
											logmsg="*** Error: the pipe #"+pipe_aim+"->#"+user_data_id_string+" does not exist yet. ***\n";
											write(select_count, logmsg.c_str(), logmsg.size());
											input_error = 1;
										}
										else{
											logmsg="*** "+user_data[select_count].user_name+" (#"+user_data_id_string+") just received from "+user_data[user_tmp_tmp].user_name+" (#"+pipe_aim+") by '"+str+"' ***\n";
											Broadcast_user(logmsg,select_count,sockfd,fdmax,master,1);
										}
										if(input_error==1){
											while(pipe(user_pipe[user_pipe_from][user_data[select_count].user_id].fd_user_pipe)<0){
												usleep(500);
											}
										}										
										user_pipe[user_pipe_from][user_data[select_count].user_id].data_empty = 0;										
										close(user_pipe[user_pipe_from][user_data[select_count].user_id].fd_user_pipe[WRITE_END]);
									}																						
									char *a[tmp_res.size()+1];
									Vector_to_Char(a,tmp_res);
				
									int pip_size = Pipenode.size();
								
									if(pip_size>0){	
										int same_count = same_count_pipe_node(Pipenode,jumpcount,user_data[select_count].user_id);	//找到跟目的地一樣的pipe將資料放進去	
										if(same_count>0){
											same_count = same_count -1;
											need_new_pipe=0;											//回傳避免0所以+1 扣回來
											if(int find_zero = find_zero_pipe_node(Pipenode,user_data[select_count].user_id)){		//有一樣目的地且有pipe要傳入
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
													if(set_input_pipe==1){
														if(input_error==1){
															dup2(read_error,STDIN_FILENO);
														}else{
															dup2(user_pipe[user_pipe_from][user_data[select_count].user_id].fd_user_pipe[READ_END],STDIN_FILENO);
														}
														close(user_pipe[user_pipe_from][user_data[select_count].user_id].fd_user_pipe[READ_END]);
													}
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
													if(set_input_pipe==1){
														close(user_pipe[user_pipe_from][user_data[select_count].user_id].fd_user_pipe[READ_END]);
													}
												}
											}
										}
										else{								
											if(int find_zero = find_zero_pipe_node(Pipenode,user_data[select_count].user_id)){		//有pipe要傳入處理	
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
													pipe_node_data.ownerID = user_data[select_count].user_id;
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
											if(set_input_pipe==1){
												if(input_error==1){
													dup2(read_error,STDIN_FILENO);
												}else{
													dup2(user_pipe[user_pipe_from][user_data[select_count].user_id].fd_user_pipe[READ_END],STDIN_FILENO);
												}
												close(user_pipe[user_pipe_from][user_data[select_count].user_id].fd_user_pipe[READ_END]);
											}
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
										if(set_input_pipe==1){
											close(user_pipe[user_pipe_from][user_data[select_count].user_id].fd_user_pipe[READ_END]);
										}												
										pipe_node_data.fd_in_pipe[READ_END] = fd[READ_END];
										pipe_node_data.fd_in_pipe[WRITE_END] = fd[WRITE_END];
										pipe_node_data.count = jumpcount;
										pipe_node_data.ownerID = user_data[select_count].user_id;
										Pipenode.push_back(pipe_node_data);												
										}
									}
							
									if(Pipenode.size()>0){												//若有等待的pipe， count--
										Pipe_Decrease(Pipenode,user_data[select_count].user_id);
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
									int_to_string << user_data[select_count].user_id;
									int_to_string >> user_data_id_string;	
								//--------------------------------------								
									if(pipe_aim.size()!='\0'){                  //\0等同null	
										if(user_pipe[user_data[select_count].user_id][user_pipe_aim].data_empty==1){	//1=full 0= empty
											logmsg="*** Error: the pipe #"+user_data_id_string+"->#"+pipe_aim+" already exists. ***\n";
											write(select_count, logmsg.c_str(), logmsg.size());
										}
										else{										
											handle_ptr = res.size();
											if(set_input_pipe==1){
												if((user_tmp_tmp = find_id_fd(user_pipe_from,sockfd,fdmax,master,user_data))<0){
													logmsg="*** Error: user #"+pipe_from+" does not exist yet. ***\n";
													write(select_count, logmsg.c_str(), logmsg.size());
													input_error = 1;
												}
												else if (user_pipe[user_pipe_from][user_data[select_count].user_id].data_empty==0){
													logmsg="*** Error: the pipe #"+pipe_from+"->#"+user_data_id_string+" does not exist yet. ***\n";
													write(select_count, logmsg.c_str(), logmsg.size());
													input_error = 1;
												}
												else{
													logmsg="*** "+user_data[select_count].user_name+" (#"+user_data_id_string+") just received from "+user_data[user_tmp_tmp].user_name+" (#"+pipe_from+") by '"+str+"' ***\n";
													Broadcast_user(logmsg,select_count,sockfd,fdmax,master,1);
												}
												if(input_error==1){
													while(pipe(user_pipe[user_pipe_from][user_data[select_count].user_id].fd_user_pipe)<0){
														usleep(500);
													}
												}													
												user_pipe[user_pipe_from][user_data[select_count].user_id].data_empty = 0;
											}											
											if((user_tmp = find_id_fd(user_pipe_aim,sockfd,fdmax,master,user_data))>0){	
																								
												int pipe_to_node = find_zero_pipe_node(Pipenode,user_data[select_count].user_id);												
												if(pipe_to_node>0){
													pipe_to_node = pipe_to_node-1;																																															
													while(pipe(user_pipe[user_data[select_count].user_id][user_pipe_aim].fd_user_pipe)<0){
														usleep(500);
													}
													close(Pipenode[pipe_to_node].fd_in_pipe[WRITE_END]);
													last_process_fork=fork_amount_check(last_process_fork);																																										
													if (last_process_fork==0){
															dup2(Pipenode[pipe_to_node].fd_in_pipe[READ_END],STDIN_FILENO);
															close(Pipenode[pipe_to_node].fd_in_pipe[READ_END]);
														//---------------------------------------------------------
															close(user_pipe[user_data[select_count].user_id][user_pipe_aim].fd_user_pipe[READ_END]);
															if(err_mark==1)dup2(user_pipe[user_data[select_count].user_id][user_pipe_aim].fd_user_pipe[WRITE_END],STDERR_FILENO);
															dup2(user_pipe[user_data[select_count].user_id][user_pipe_aim].fd_user_pipe[WRITE_END],STDOUT_FILENO);
															close(user_pipe[user_data[select_count].user_id][user_pipe_aim].fd_user_pipe[WRITE_END]);	

															if(execvp(file_content[0],file_content)<0){
																cerr << "Unknown command: ["<<file_content[0]<<"]."<<endl;
																exit(1);
															}						
														}
													else{
														close(Pipenode[pipe_to_node].fd_in_pipe[READ_END]);
														user_pipe[user_data[select_count].user_id][user_pipe_aim].data_empty=1;
														int status;
														waitpid(last_process_fork, &status, 0);							
													}							
												}
												else{
													while(pipe(user_pipe[user_data[select_count].user_id][user_pipe_aim].fd_user_pipe)<0){
														usleep(500);
													}
													if(set_input_pipe==1){
														close(user_pipe[user_pipe_from][user_data[select_count].user_id].fd_user_pipe[WRITE_END]);
													}
													last_process_fork=fork_amount_check(last_process_fork);
													if (last_process_fork==0){
															if(set_input_pipe==1){
																if(input_error==1){
																	dup2(read_error,STDIN_FILENO);
																}else{
																	dup2(user_pipe[user_pipe_from][user_data[select_count].user_id].fd_user_pipe[READ_END],STDIN_FILENO);
																}
																close(user_pipe[user_pipe_from][user_data[select_count].user_id].fd_user_pipe[READ_END]);
															}
															close(user_pipe[user_data[select_count].user_id][user_pipe_aim].fd_user_pipe[READ_END]);
															dup2(user_pipe[user_data[select_count].user_id][user_pipe_aim].fd_user_pipe[WRITE_END],STDOUT_FILENO);
															close(user_pipe[user_data[select_count].user_id][user_pipe_aim].fd_user_pipe[WRITE_END]);
															if(execvp(file_content[0],file_content)<0){
																cerr << "Unknown command: ["<<file_content[0]<<"]."<<endl;
																exit(1);
															}						
														}
													else{
														if(set_input_pipe==1){
															close(user_pipe[user_pipe_from][user_data[select_count].user_id].fd_user_pipe[READ_END]);
														}	
														user_pipe[user_data[select_count].user_id][user_pipe_aim].data_empty=1;
													}
												}
												logmsg="*** "+user_data[select_count].user_name+" (#"+user_data_id_string+") just piped '"+str+"' to "+user_data[user_tmp].user_name+" (#"+pipe_aim+") ***\n";												
												Broadcast_user(logmsg,select_count,sockfd,fdmax,master,1);
											}
											else{
												logmsg="*** Error: user #"+pipe_aim+" does not exist yet. ***\n";
												write(select_count, logmsg.c_str(), logmsg.size());
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
										int pipe_to_node = find_zero_pipe_node(Pipenode,user_data[select_count].user_id);
										
										
										if(set_input_pipe==1){
											if((user_tmp_tmp = find_id_fd(user_pipe_from,sockfd,fdmax,master,user_data))<0){
												logmsg="*** Error: user #"+pipe_from+" does not exist yet. ***\n";
												write(select_count, logmsg.c_str(), logmsg.size());
												input_error = 1;
											}
											else if (user_pipe[user_pipe_from][user_data[select_count].user_id].data_empty==0){
												logmsg="*** Error: the pipe #"+pipe_from+"->#"+user_data_id_string+" does not exist yet. ***\n";
												write(select_count, logmsg.c_str(), logmsg.size());
												input_error = 1;
											}
											else{
												logmsg="*** "+user_data[select_count].user_name+" (#"+user_data_id_string+") just received from "+user_data[user_tmp_tmp].user_name+" (#"+pipe_from+") by '"+str+"' ***\n";
												Broadcast_user(logmsg,select_count,sockfd,fdmax,master,1);
											}													
											user_pipe[user_pipe_from][user_data[select_count].user_id].data_empty = 0;
										}										
										
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
											if(set_input_pipe==1){
												close(user_pipe[user_pipe_from][user_data[select_count].user_id].fd_user_pipe[WRITE_END]);
											}
											last_process_fork=fork_amount_check(last_process_fork);
											if (last_process_fork==0){
												if(set_input_pipe==1){
													if(input_error==1){
														dup2(read_error,STDIN_FILENO);
													}else{
														dup2(user_pipe[user_pipe_from][user_data[select_count].user_id].fd_user_pipe[READ_END],STDIN_FILENO);
													}
													close(user_pipe[user_pipe_from][user_data[select_count].user_id].fd_user_pipe[READ_END]);
												}
												dup2(write_port,STDOUT_FILENO);
												if(execvp(file_content[0],file_content)<0){
													cerr << "Unknown command: ["<<file_content[0]<<"]."<<endl;
													exit(1);
												}						
											}
											else{
												if(set_input_pipe==1){
													close(user_pipe[user_pipe_from][user_data[select_count].user_id].fd_user_pipe[READ_END]);
												}
												int status;
												waitpid(last_process_fork, &status, 0);
											}
										}												
									}
									if(Pipenode.size()>0){						//若有等待的pipe， count--
										Pipe_Decrease(Pipenode,user_data[select_count].user_id);
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
											int_to_string << user_data[select_count].user_id;
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
										if((user_tmp_tmp = find_id_fd(user_pipe_from,sockfd,fdmax,master,user_data))<0){
											logmsg="*** Error: user #"+pipe_from+" does not exist yet. ***\n";
											write(select_count, logmsg.c_str(), logmsg.size());
											input_error = 1;
										}
										else if (user_pipe[user_pipe_from][user_data[select_count].user_id].data_empty==0){
											logmsg="*** Error: the pipe #"+pipe_from+"->#"+user_data_id_string+" does not exist yet. ***\n";
											write(select_count, logmsg.c_str(), logmsg.size());
											input_error = 1;
										}
										else{
											logmsg="*** "+user_data[select_count].user_name+" (#"+user_data_id_string+") just received from "+user_data[user_tmp_tmp].user_name+" (#"+pipe_from+") by '"+str+"' ***\n";
											Broadcast_user(logmsg,select_count,sockfd,fdmax,master,1);
										}
										if(input_error==1){
											while(pipe(user_pipe[user_pipe_from][user_data[select_count].user_id].fd_user_pipe)<0){
												usleep(500);
											}
										}										
										user_pipe[user_pipe_from][user_data[select_count].user_id].data_empty=0;
									}
									int pipe_to_node = find_zero_pipe_node(Pipenode,user_data[select_count].user_id);
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
										if(set_input_pipe==1){
											close(user_pipe[user_pipe_from][user_data[select_count].user_id].fd_user_pipe[WRITE_END]);
										}
										last_process_fork=fork_amount_check(last_process_fork);
										if (last_process_fork==0){
											if(set_input_pipe==1){
												if(input_error==1){
													dup2(read_error,STDIN_FILENO);
												}else{
													dup2(user_pipe[user_pipe_from][user_data[select_count].user_id].fd_user_pipe[READ_END],STDIN_FILENO);
												}
												close(user_pipe[user_pipe_from][user_data[select_count].user_id].fd_user_pipe[READ_END]);
											}
											if(execvp(b[0],b)<0){
												cerr << "Unknown command: ["<<b[0]<<"]."<<endl;
												exit(1);
											}									
										}
										else{
											if(set_input_pipe==1){
												close(user_pipe[user_pipe_from][user_data[select_count].user_id].fd_user_pipe[READ_END]);
											}											
											int status;
											waitpid(last_process_fork, &status, 0);
										}
									}						
									if(Pipenode.size()>0){						//若有等待的pipe， count--
										Pipe_Decrease(Pipenode,user_data[select_count].user_id);
									}						
									
								}
							
							}
							set_input_pipe = 0;
							input_error=0;
							err_mark=0;
							handle_ptr++;							
					}	
					handle_ptr=0;
					head=0;
					write(user_data[select_count].user_fd,"% ",2);	
				}				
			}
		}		
	}
}
	



