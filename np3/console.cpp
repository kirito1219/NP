#include <array>
#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/format.hpp>
#include <fstream>
#include <iostream>
#include <string>

using namespace boost::asio;
using namespace boost::asio::ip;
using namespace std;

io_service global_io_service;



struct hosts{
	string name;
	string port;
	string file;	
}Host[5];

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

void output_string(){
	cout << "Content-type: text/html\r\n\r\n";
	string html_string =
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
	cout << html_string + (fmt%table%session).str() + "</html>\n" <<endl;
	cout.flush();
}


void replace_content(string &data) {
	using boost::algorithm::replace_all;
	replace_all(data, "&", "&amp;");
    replace_all(data, "\"", "&quot;");
    replace_all(data, "\'", "&apos;");
    replace_all(data, "<", "&lt;");
    replace_all(data, ">", "&gt;");
}



void output_shell(string session, string content) {
	replace_content(content);
	boost::replace_all(content, "\n", "&#13;");
    boost::replace_all(content, "\r", "");
	boost::format fmt
		("<script>document.getElementById('%1%').innerHTML += '%2%';</script>");
	cout << fmt%session%content << endl;
	cout.flush();

}

void output_command(string session, string content) {
	replace_content(content);
	boost::replace_all(content, "\n", "&#13;");
    boost::replace_all(content, "\r", "");
	boost::format fmt
		("<script>document.getElementById('%1%').innerHTML += '<b>%2%</b>';</script>");
	cout << fmt%session%content << endl;
	cout.flush();
}



struct Client : public enable_shared_from_this<Client> {
  Client(string session, string file_name, tcp::resolver::query q)
      : session(move(session)), q(move(q)) {
    file.open("test_case/" + file_name, ios::in);
  }

		
  void write_handler(string common) {
    auto self(shared_from_this());
    tcp_socket.async_send(buffer(common),[this, self](boost::system::error_code ec,size_t length) {
        });
  }

  void read_handler() {
    auto self(shared_from_this());
    tcp_socket.async_receive(buffer(bytes),
                             [this, self](const boost::system::error_code &ec,size_t length) {
                               if (!ec) {								
                                string data(bytes.begin(),bytes.begin()+ length);
                                output_shell(session, data);
								
								if(data.find("% ")!=string::npos) {
									string command;
									getline(file, command);
									write_handler(command + '\n');
									//tcp_socket.write_some(buffer(command + '\n'));
									output_command(session, command + '\n');									
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
  tcp::resolver resolver{global_io_service};
  tcp::socket tcp_socket{global_io_service};
  array<char, 15000> bytes;
  fstream file; 
  

};



int main(){
	string str = getenv("QUERY_STRING");	
	splitAndParse(str);
	output_string();
	for (int i = 0; i < 5; ++i) {
	  if(Host[i].name!=""){
		tcp::resolver::query q(Host[i].name, Host[i].port);
		make_shared<Client>("s" + to_string(i), Host[i].file, move(q))
          ->resolve();
	  }
	}
	global_io_service.run();
}