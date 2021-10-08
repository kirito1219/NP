#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp> 
#include <sys/types.h>
#include <sys/wait.h>
#include <cstdlib>
#include <iostream>
#include <string>
#include <unistd.h>


using boost::asio::ip::tcp;
using namespace std;

class http_server
{
public:
  http_server(boost::asio::io_service& io_service, unsigned short port)
    : _io_service(io_service),
      server_signal(io_service, SIGCHLD),
      server_acceptor(io_service, tcp::endpoint(tcp::v4(), port)),
      server_socket(io_service)
  {
    signal_wait();
    start_accept();
  }

private:
  void signal_wait()
  {
    server_signal.async_wait([this](boost::system::error_code /**ec*/, int /**signo*/){
		if (server_acceptor.is_open())
		{
		  // Reap completed child processes so that we don't end up with zombies.
		  int status = 0;
		  while (waitpid(-1, &status, WNOHANG) > 0) {}

		  signal_wait();
		}
	});
  }

  void start_accept()
  {
		server_acceptor.async_accept([this](boost::system::error_code ec, tcp::socket new_socket) 
		{
		if (!ec)
		{
			server_socket = move(new_socket);
			_io_service.notify_fork(boost::asio::io_service::fork_prepare);

			if (fork() == 0)
			{
				_io_service.notify_fork(boost::asio::io_service::fork_child);
				server_acceptor.close();
				server_signal.cancel();

				start_read();
			}
			else
			{
				_io_service.notify_fork(boost::asio::io_service::fork_parent);

				server_socket.close();
				start_accept();
			}
		}
		else
		{
			std::cerr << "Accept error: " << ec.message() << std::endl;
			start_accept();
		}

		});
	}
	string setAndSplit(string parse_env){
		string target;
		vector<string> spilt_input,result,split_part,split_host_name;
		boost::split(spilt_input, parse_env, boost::is_any_of("\r\n"));			
		boost::split(split_part, spilt_input[0], boost::is_any_of(" "));
		setenv("REQUEST_METHOD", split_part[0].c_str(), 1);
		boost::split(result, split_part[1], boost::is_any_of("?"));
		if(result.size()>1){
			setenv("REQUEST_URI", result[0].c_str(), 1);
			target = result[0];
			setenv("QUERY_STRING", result[1].c_str(), 1);
		}
		else{
			setenv("REQUEST_URI", split_part[1].c_str(), 1);
			target = split_part[1];
			setenv("QUERY_STRING", "", 1);
		}
		setenv("SERVER_PROTOCOL", split_part[2].c_str(), 1);
		boost::split(split_part, spilt_input[2], boost::is_any_of(":"));
		setenv("HTTP_HOST", split_part[1].substr(1).c_str(), 1);
		setenv("SERVER_ADDR",server_socket.local_endpoint().address().to_string().c_str(), 1);
		setenv("SERVER_PORT",to_string(server_socket.local_endpoint().port()).c_str(), 1);
		setenv("REMOTE_ADDR",server_socket.remote_endpoint().address().to_string().c_str(), 1);
		setenv("REMOTE_PORT",to_string(server_socket.remote_endpoint().port()).c_str(), 1);
		return target;
	}
	

	void start_read() {
		server_socket.async_receive(boost::asio::buffer(_data),[this](boost::system::error_code ec,size_t length) {
		if (!ec) {
			auto parse_env = string(_data.begin(), _data.begin() + length);
			string target = setAndSplit(parse_env);									
			dup2(server_socket.native_handle(), 0);
			dup2(server_socket.native_handle(), 1);
			dup2(server_socket.native_handle(), 2);
			cout << "HTTP/1.1" << " 200 OK\r\n";
			cout.flush();			
			target = target.substr(1);
			char *argv[] = {nullptr};
			if (execv(target.c_str(), argv)>0) {
			  cerr<< "error connect" << endl;
			  exit(1);
			}
		}
		});
	}
  boost::asio::io_service& _io_service;
  boost::asio::signal_set server_signal;
  tcp::acceptor server_acceptor;
  tcp::socket server_socket;
  array<char, 15000> _data;
};

int main(int argc, char* argv[])
{
  try
  {
    if (argc != 2)
    {
      std::cerr << "Usage: process_per_connection <port>\n";
      return 1;
    }
    boost::asio::io_service io_service;
    http_server s(io_service, atoi(argv[1]));
    io_service.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << std::endl;
  }
}