#ifndef LIB_NETSOCKET_FTP_H
#define LIB_NETSOCKET_FTP_H
#include <string>
#include <vector>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h> 
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <fstream>


class ftp_t 
{
public:


  enum FTP_STATUS {
      SERVER_CONNECTION_ERROR = 0x0,  
      SERVER_CONNECTION_SUCCESS = 0x1,
      SERVER_LOGIN_SUCCESS = 0x2,   // User & Pass correct
      SERVER_LOGIN_ERROR = 0x3,    // User & Pass incorrect
      SERVER_PPERMISSION_DENIED = 0x4,  // When user doesn't have a permission on file or dir (Writing,Reading,Appending,Del,....)
      SERVER_FILE_NOT_FOUND = 0x5, // File not found on ftp server side 
      CLIENT_FILE_NOT_FOUND = 0x6 // File not found on client side

  };


  // set ftp server info
  ftp_t(const char *host_name, const unsigned short server_port);
  int login(const char *user_name, const char *pass); // Log into server 
  int logout(); // Quit server
  std::vector<std::string>  get_file_list(); // Get dir and files list in root directory +
  int get_file(const char *file_name); // Get a file from ftp server +a
  int send_file(const char *file_name); // Send a file to ftp server
  std::string get_last_sv_response(); // Get last ftp server response 
  int is_connected(); // Check if connection is established



  // Utils ->
  int get_bin_size(const char *filename); // Get file size on client disk


private:
   
  std::string m_server_ip;  // Ftp server ip addr
  unsigned short m_server_port = 21; // Ftp server port
  std::string m_username;  // Username for ftp server
  std::string m_password;  // Password for ftp server
  int sock_ctrl; // Control socket (For sending commands)
  int sock_data; // Data socket (For send() / recv() ) in passive mode
  int server_status = 0; // used by is_connected() for testing
  std::string last_rsp = "-"; // Last server response
  int m_error_code =0 ;  // Last server error code FTP_STATUS


  // Create connection between client and ftp server ->
  int create_socket(int &sock, const char* server_ip, const unsigned short server_port);
  std::vector<std::string>  receive_list(int sock);
  std::string  get_response(int sock, std::string &str_rep); // Get response from server after using send_request
  int send_request(int sock, const char* buf_request); // Send command to server
  // Parse PASV mode ip address
  int parse_PASV_response(const std::string &str_rsp, std::string &str_server_ip, unsigned short &server_port); 
  int close_socket(int sock); // Close connection 
  int receive_all(int sock, const char *file_name); // Used for receiving a file from server 
  int send_all(int sock, const void *vbuf, int size_buf);
  int send_binary(int sock, const char *file_name); // Used for sending  a file from server 


};




#endif

