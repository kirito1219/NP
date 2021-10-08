#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp> 
#include <fstream>
#include <cstdlib>
#include <iostream>
#include <string>
#include <unistd.h>

using boost::asio::ip::tcp;
using namespace std;

boost::asio::io_service io_service;
string str;


struct hosts{
	string name;
	string port;
	string file;	
}Host[5];

class cgiSession : public enable_shared_from_this<cgiSession> {
	public:
	class Client : public enable_shared_from_this<Client> {
		public:
	  Client(shared_ptr<cgiSession> ptr,string session, string file_name, tcp::resolver::query q)
		  : ptr_(ptr),session(move(session)), q(move(q)) {
		 file_.open("test_case/" + file_name, ios::in);
	  }

		void replace_content(string &data) {
			using boost::algorithm::replace_all;
			replace_all(data, "&", "&amp;");
			replace_all(data, "\"", "&quot;");
			replace_all(data, "\'", "&apos;");
			replace_all(data, "<", "&lt;");
			replace_all(data, ">", "&gt;");
		}



		string output_shell(string session, string content) {
			replace_content(content);
			boost::replace_all(content, "\n", "&#13;");
			boost::replace_all(content, "\r", "");
			boost::format fmt("<script>document.getElementById('%1%').innerHTML += '%2%';</script>");
			content = (fmt%session%content).str();	
			return content;
		}

		string output_command(string session, string content) {
			replace_content(content);
			boost::replace_all(content, "\n", "&#13;");
			boost::replace_all(content, "\r", "");
			boost::format fmt("<script>document.getElementById('%1%').innerHTML += '<b>%2%</b>';</script>");
			content = (fmt%session%content).str();	
			return content;
		}
	 

			
	  void write_handler(string common) {
		auto self(shared_from_this());
		tcp_socket.async_send(boost::asio::buffer(common),
			[this, self](boost::system::error_code ec,size_t length) {
			});
	  }

	  void read_handler() {
		auto self(shared_from_this());
		tcp_socket.async_receive(boost::asio::buffer(bytes),
								 [this, self](const boost::system::error_code &ec,
											  size_t length) {
								   if (!ec) {								
									string data(bytes.begin(),bytes.begin()+ length);
									boost::asio::write(ptr_->_socket,boost::asio::buffer(output_shell(session,data)));
									if(data.find("% ")!=std::string::npos) {
										string command;
										getline(file_, command);
										boost::asio::write(ptr_->_socket,boost::asio::buffer(output_command(session, command + '\n')));																		
										write_handler(command + '\n');
									}
									read_handler();
								   }
								 }
								);
	  }

	  void connect_handler() {
		read_handler();
	  }

	  void resolve_handler(tcp::resolver::iterator it) {
		auto self(shared_from_this());
		tcp_socket.async_connect(*it, [this, self](const boost::system::error_code &ec) {
		  if (!ec)
			connect_handler();
		});
	  }

	  void resolve() {
		auto self(shared_from_this());
		resolver.async_resolve(q,[this, self](const boost::system::error_code &ec,tcp::resolver::iterator it) {
								 if (!ec)
								   resolve_handler(it);
							   });
	  }
	  string session;
	  tcp::resolver::query q;
	  tcp::resolver resolver{io_service};
	  tcp::socket tcp_socket{io_service};
	  array<char, 15000> bytes;
	  fstream file_; 
	  shared_ptr<cgiSession> ptr_;
	};

	
public:	
	cgiSession(tcp::socket socket) : _socket(move(socket)) {}		 
	void start(){ do_read(); }
	
	
		
private:
	void splitAndParse(string str){
		istringstream delim(str);
		string token;
		char attribute;
		int count = 0; 
		while(getline(delim,token,'&')){
			attribute = token.front();
			if(attribute=='h')
				Host[count].name=token.substr(token.find('=')+1);
			else if (attribute=='p')
				Host[count].port=token.substr(token.find('=')+1);		
			else if (attribute=='f'){
				Host[count].file=token.substr(token.find('=')+1);
				count++;
			}
		}
	}
 
	string setAndSplit(string parse_env){
		string target;
		vector<string> spilt_input,result,split_part;
		boost::split(spilt_input, parse_env, boost::is_any_of("\r\n"));			
		boost::split(split_part, spilt_input[0], boost::is_any_of(" "));
		boost::split(result, split_part[1], boost::is_any_of("?"));		
		if(result.size()>1){
			target = result[0];
			str = result[1];
		}
		else{
			target = split_part[1];
		}	
		return target;
	}
 
	void do_write(string content) {
    auto self(shared_from_this());
    _socket.async_send(boost::asio::buffer(content),
        [this, self](boost::system::error_code ec,size_t length) {
        });
  }
 
	void do_read() {
    auto self(shared_from_this());
    _socket.async_receive(boost::asio::buffer(_data),[this, self](boost::system::error_code ec, size_t length) {
			if (!ec){
				auto parse_env = string(_data.begin(), _data.begin() + length);
				string target = setAndSplit(parse_env);				
				target = target.substr(1);
				string content;
				if(target=="panel.cgi"){
					content = panel_all_string();
					do_write(content);
				}else if(target=="console.cgi"){
					splitAndParse(str);
					content = console_all_string();
					do_write(content);
					for (int i = 0; i < 5; ++i) {
					  if(Host[i].name!=""){
						tcp::resolver::query q{Host[i].name, Host[i].port};
						make_shared<Client>(self,"s" + to_string(i), Host[i].file, move(q))->resolve();
					  }
					}
				}
			}			  
        });
	}
	string console_all_string(){
		string string_html =
        "HTTP/1.1 200 OK\r\n"
        "Content-type: text/html\r\n\r\n";
		string_html += console_output_string();
		return string_html;
	}
	string console_output_string(){
		string console_html_string =
				"<!DOCTYPE html>\n"
				"<html lang=\"en\">\n"
				"<head>\n"
				"<meta charset=\"UTF-8\" />\n"
				"<title>NP Project 3 Console</title>\n"
				"<link\n"
				"rel=\"stylesheet\"\n"
				"href=\"https://stackpath.bootstrapcdn.com/bootstrap/4.1.3/css/bootstrap.min.css\"\n"
				"integrity=\"sha384-MCw98/SFnGE8fJT3GXwEOngsV7Zt27NXFoaoApmYm81iuXoPkFOJwJ8ERdknLPMO\"\n"
				"crossorigin=\"anonymous\"\n"
				"/>\n"
				"<link\n"
				"href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\"\n"
				"rel=\"stylesheet\"\n"
				"/>\n"
				"<link\n"
				"rel=\"icon\"\n"
				"type=\"image/png\"\n"
				"href=\"https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png\"\n"
				"/>\n"
				"<style>\n"
				"* {\n"
				"font-family: 'Source Code Pro', monospace;\n"
				"font-size: 1rem !important;\n"
				"}\n"
				"body {\n"
				"background-color: #212529;\n"
				"}\n"
				"pre {\n"
				"color: #cccccc;\n"
				"}\n"
				"b {\n"
				"color: #ffffff;\n"
				"}\n"
				"</style>\n"
				"</head>\n";

		boost::format fmt("<body>\n"
						  "<table class=\"table table-dark table-bordered\">\n"
						  "<thead>\n"
						  "<tr>\n"
						  "%1%"
						  "</tr>\n"
						  "</thead>\n"
						  "<tbody>\n"
						  "<tr>\n"
						  "%2%"
						  "</tr>\n"
						  "</tbody>\n"
						  "</table>\n"
						  "</body>\n");

		std::string table;
		std::string session;
		for (int i = 0; i < 5; ++i) {
		  if(Host[i].name!=""){
			table +="<th scope=\"col\">" + Host[i].name + ":" + Host[i].port + "</th>\n";
			session += "<td><pre id=\"s" + to_string(i)+ "\"class=\"mb-0\"></pre></td>";
		  }
		}
		console_html_string += (fmt%table%session).str() + "</html>\n" ;
		return console_html_string;
	}
	
	
	string panel_output_string(){
		string panel_string =
        "<!DOCTYPE html>\n"
        "<html lang=\"en\">\n"
        "  <head>\n"
        "    <title>NP Project 3 Panel</title>\n"
        "    <link\n"
        "      rel=\"stylesheet\"\n"
        "	   href=\"https://stackpath.bootstrapcdn.com/bootstrap/4.1.3/css/bootstrap.min.css\"\n"
        "	   integrity=\"sha384-MCw98/SFnGE8fJT3GXwEOngsV7Zt27NXFoaoApmYm81iuXoPkFOJwJ8ERdknLPMO\"\n"
        "      crossorigin=\"anonymous\"\n"
        "    />\n"
        "    <link\n"
        "      "
        "	   href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\"\n"
        "      rel=\"stylesheet\"\n"
        "    />\n"
        "    <link\n"
        "      rel=\"icon\"\n"
        "      type=\"image/png\"\n"
        "      "
        "		href=\"https://cdn4.iconfinder.com/data/icons/iconsimple-setting-time/512/dashboard-512.png\"\n"
        "    />\n"
        "    <style>\n"
        "      * {\n"
        "        font-family: 'Source Code Pro', monospace;\n"
        "      }\n"
        "    </style>\n"
        "  </head>\n"
        "  <body class=\"bg-secondary pt-5\">"
        "  <form action=\"console.cgi\" method=\"GET\">\n"
        "      <table class=\"table mx-auto bg-light\" style=\"width: inherit\">\n"
        "        <thead class=\"thead-dark\">\n"
        "          <tr>\n"
        "            <th scope=\"col\">#</th>\n"
        "            <th scope=\"col\">Host</th>\n"
        "            <th scope=\"col\">Port</th>\n"
        "            <th scope=\"col\">Input File</th>\n"
        "          </tr>\n"
        "        </thead>\n"
        "        <tbody>";

		boost::format fmt(
			"          <tr>\n"
			"            <th scope=\"row\" class=\"align-middle\">Session %1%</th>\n"
			"            <td>\n"
			"              <div class=\"input-group\">\n"
			"                <select name=\"h%3%\" class=\"custom-select\">\n"
			"                  <option></option>{%2%}\n"
			"                </select>\n"
			"                <div class=\"input-group-append\">\n"
			"                  <span "
			"					class=\"input-group-text\">.cs.nctu.edu.tw</span>\n"
			"                </div>\n"
			"              </div>\n"
			"            </td>\n"
			"            <td>\n"
			"              <input name=\"p%3%\" type=\"text\" "
			"				class=\"form-control\" size=\"5\" />\n"
			"            </td>\n"
			"            <td>\n"
			"              <select name=\"f%3%\" class=\"custom-select\">\n"
			"                <option></option>\n"
			"               %4%\n"
			"              </select>\n"
			"            </td>\n"
			"          </tr>");

		for (int i = 0; i < 5; ++i) {
		  panel_string += (fmt % (i + 1) % host_menu() % i % test_case_menu()).str();
		}

		panel_string +=
			"          <tr>\n"
			"            <td colspan=\"3\"></td>\n"
			"            <td>\n"
			"              <button type=\"submit\" class=\"btn btn-info btn-block\">Run</button>\n"
			"            </td>\n"
			"          </tr>\n"
			"        </tbody>\n"
			"      </table>\n"
			"    </form>\n"
			"  </body>\n"
			"</html>";
		return panel_string;
	}	

	string host_menu() {
    vector<string> hosts_menu = {
        "<option value=\"nplinux1.cs.nctu.edu.tw\">nplinux1</option>",
        "<option value=\"nplinux2.cs.nctu.edu.tw\">nplinux2</option>",
        "<option value=\"nplinux3.cs.nctu.edu.tw\">nplinux3</option>",
        "<option value=\"nplinux4.cs.nctu.edu.tw\">nplinux4</option>",
        "<option value=\"nplinux5.cs.nctu.edu.tw\">nplinux5</option>",
        "<option value=\"nplinux6.cs.nctu.edu.tw\">nplinux6</option>",
        "<option value=\"nplinux7.cs.nctu.edu.tw\">nplinux7</option>",
        "<option value=\"nplinux8.cs.nctu.edu.tw\">nplinux8</option>",
        "<option value=\"nplinux9.cs.nctu.edu.tw\">nplinux9</option>",
        "<option value=\"nplinux10.cs.nctu.edu.tw\">nplinux10</option>"
		"<option value=\"nplinux11.cs.nctu.edu.tw\">nplinux11</option>",
        "<option value=\"nplinux12.cs.nctu.edu.tw\">nplinux12</option>"};

		return string(boost::algorithm::join(hosts_menu, ""));
    }

    string test_case_menu() {
    vector<string> test_case_menu = {
        "<option value=\"t1.txt\">t1.txt</option>",
        "<option value=\"t2.txt\">t2.txt</option>",
        "<option value=\"t3.txt\">t3.txt</option>",
        "<option value=\"t4.txt\">t4.txt</option>",
        "<option value=\"t5.txt\">t5.txt</option>",
        "<option value=\"t6.txt\">t6.txt</option>",
        "<option value=\"t7.txt\">t7.txt</option>",
        "<option value=\"t8.txt\">t8.txt</option>",
        "<option value=\"t9.txt\">t9.txt</option>",
        "<option value=\"t10.txt\">t10.txt</option>"};

		return string(boost::algorithm::join(test_case_menu, ""));
    }
		
	string panel_all_string() {
    auto self(shared_from_this());
    string string_html =
        "HTTP/1.1 200 OK\r\n"
        "Content-type: text/html\r\n\r\n";
    string_html += panel_output_string();
    return string_html;
	}
  
  
  
  
	tcp::socket _socket;
	array<char, 15000> _data;	
};



class cgi_server
{
public:
	cgi_server(boost::asio::io_service& io_service, unsigned short port)
    : server_acceptor(io_service, tcp::endpoint(tcp::v4(), port)),
	  server_socket(io_service)
	{
		start_accept();
	}

private:

  void start_accept()
  {
		server_acceptor.async_accept(server_socket,[this](boost::system::error_code ec) 
		{
			if (!ec)
			{
				std::make_shared<cgiSession>(std::move(server_socket))->start();
			}
		start_accept();
		});
	}		
	tcp::acceptor server_acceptor;
	tcp::socket server_socket;
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

    cgi_server s(io_service, atoi(argv[1]));
    io_service.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << std::endl;
  }
}