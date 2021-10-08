#include <sys/wait.h>
#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/ip/network_v4.hpp>
#include <boost/bind.hpp>
#include <boost/format.hpp>
#include <fstream>
#include <iostream>
#include "socks_class.hpp"

using namespace std;
using boost::asio::ip::tcp;

class sock4_server
{
public:
  sock4_server(boost::asio::io_service& io_service, unsigned short port)
    : _io_service(io_service),
	  resolver_(io_service),
      server_signal(io_service, SIGCHLD),
      server_acceptor(io_service, tcp::endpoint(tcp::v4(), port)),
      source_socket(io_service),
	  destination_socket(io_service)
  {
    start_signal_wait();
    start_accept();
  }

 private:
  void start_signal_wait()
  {
    server_signal.async_wait([this](boost::system::error_code /**ec*/, int /**signo*/){
		if (server_acceptor.is_open())
		{
		  // Reap completed child processes so that we don't end up with zombies.
		  int status = 0;
		  while (waitpid(-1, &status, WNOHANG) > 0) {}

		  start_signal_wait();
		}
	});
  }
  void start_accept()
  {
		server_acceptor.async_accept([this](boost::system::error_code ec, tcp::socket new_socket) 
		{
		if (!ec)
		{
			source_socket = move(new_socket);
			_io_service.notify_fork(boost::asio::io_service::fork_prepare);

			if (fork() == 0)
			{
				_io_service.notify_fork(boost::asio::io_service::fork_child);
				server_acceptor.close();
				server_signal.cancel();
				socks4_connect();
			}
			else
			{
				_io_service.notify_fork(boost::asio::io_service::fork_parent);

				source_socket.close();
				destination_socket.close();
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

	void socks4_connect() {
		boost::asio::read(source_socket, socks_req_.buffers());
		string D_IP, D_PORT, Command;
		
		D_PORT = to_string(socks_req_.getPort());
		D_IP = socks_req_.getAddress();
		Command = socks_req_.getCommand();

		if (!list_firewall(D_IP)) {
			cout << get_format() % D_IP % D_PORT % Command %
					source_socket.remote_endpoint().address().to_string() %
					source_socket.remote_endpoint().port() % "Reject";
			socks4::reply_builder reply(socks4::reply_builder::Reject, socks_req_);
			boost::asio::write(source_socket, reply.buffers());
			return;
		}

		cout << get_format() % D_IP % D_PORT % Command %
					source_socket.remote_endpoint().address().to_string() %
					source_socket.remote_endpoint().port() % "Accept";

		if (socks_req_.command_ == socks4::request_builder::command_type::connect)
			resolve(tcp::resolver::query(socks_req_.getAddress(),to_string(socks_req_.getPort())));

		else if (socks_req_.command_ == socks4::request_builder::command_type::bind) {
			
			tcp::acceptor acceptor_for_bind_(_io_service, tcp::endpoint(tcp::v4(), 0));
			
			socks_req_.port_high_byte_ = acceptor_for_bind_.local_endpoint().port() >> 8;
			socks_req_.port_low_byte_ = acceptor_for_bind_.local_endpoint().port();

			socks_req_.address_ = boost::asio::ip::make_address_v4("0.0.0.0").to_bytes();

			socks4::reply_builder reply(socks4::reply_builder::Accept, socks_req_);
			boost::asio::write(source_socket, reply.buffers());

			acceptor_for_bind_.accept(destination_socket);

			socks4::reply_builder reply_second(socks4::reply_builder::Accept, socks_req_);
			boost::asio::write(source_socket, reply_second.buffers());

			async_read_from_destination();
			async_read_from_source();
		}
	}
	
	bool list_firewall(string D_IP) {
		ifstream firewall_stream("./socks.conf");
		string line;
		vector<string> white_list;
		string criteria = "permit c ";

		if (socks_req_.command_ == socks4::request_builder::command_type::connect){
		  criteria = "permit c ";
		} else if (socks_req_.command_ == socks4::request_builder::command_type::bind) {
		  criteria = "permit b ";
		}

		while (getline(firewall_stream, line)) {
		  if (line.substr(0, criteria.size()) == criteria) {
			white_list.push_back(line.substr(criteria.size()));
		  }
		}

		for (int i=0; i < white_list.size() ; i++) {
		  string prefix = white_list[i].c_str();
		  prefix = prefix.substr(0, prefix.find('*'));
		  if (D_IP.substr(0, prefix.size()) == prefix) {
			return true;
		  }
		}

		return false;
	}
	boost::format get_format() {
		return boost::format(
			"<S_IP>:%4%\n"
			"<S_PORT>:%5%\n"
			"<D_IP>:%1%\n"
			"<D_PORT>:%2%\n"
			"<Command>:%3%\n"
			"<Reply>:%6%\n\n");
	}
	
	void async_read_from_destination() {
		destination_socket.async_receive(
			boost::asio::buffer(destination_buffer_),
			[this](boost::system::error_code ec, std::size_t length) {
			  if (!ec) {
				async_write_to_source(length);
			  } else {
				throw system_error{ec};
			  }
			});
	  }

	  void async_write_to_source(size_t length) {
		boost::asio::async_write(
			source_socket, boost::asio::buffer(destination_buffer_, length),
			[this](boost::system::error_code ec, std::size_t length) {
			  if (!ec) {
				async_read_from_destination();
			  } else {
				throw system_error{ec};
			  }
			});
	  }

	  void async_read_from_source() {
		source_socket.async_receive(
			boost::asio::buffer(source_buffer_),
			[this](boost::system::error_code ec, std::size_t length) {
			  if (!ec) {
				async_write_to_destination(length);
			  } else {
				throw system_error{ec};
			  }
			});
	  }

	  void async_write_to_destination(size_t length) {
		boost::asio::async_write(
			destination_socket, boost::asio::buffer(source_buffer_, length),
			[this](boost::system::error_code ec, std::size_t length) {
			  if (!ec) {
				async_read_from_source();
			  } else {
				throw system_error{ec};
			  }
			});
	  }
	
	
	
	
	void connect_handler() {
		socks4::reply_builder reply(socks4::reply_builder::Accept, socks_req_);
        boost::asio::write(source_socket, reply.buffers());
		async_read_from_destination();
        async_read_from_source();
	  }

	void resolve_handler(tcp::resolver::iterator it) {
		destination_socket.async_connect(*it, [this](const boost::system::error_code &ec) {
		  if (!ec)
			connect_handler();
		});
	}

	void resolve(const tcp::resolver::query& q) {
		resolver_.async_resolve(q,[this](const boost::system::error_code &ec,tcp::resolver::iterator it) {
								 if (!ec)
								   resolve_handler(it);
							   });
	}
	
	
  boost::asio::io_service& _io_service;
  boost::asio::signal_set server_signal;
  tcp::resolver resolver_;
  tcp::acceptor server_acceptor;
  tcp::socket source_socket;
  tcp::socket destination_socket;
  array<unsigned char, 65536> source_buffer_{};
  array<unsigned char, 65536> destination_buffer_{};  
  socks4::request_builder socks_req_;
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
    sock4_server s(io_service, atoi(argv[1]));
    io_service.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << std::endl;
  }
}