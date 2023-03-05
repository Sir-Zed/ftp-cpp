#include "ftp.hh"

// FTP uses two TCP connections to transfer files : a control connection and a data connection
// connect a socket(control socket) to a ftp server on the port 21
// receive on the socket a message from the ftp server(code : 220)
// send login to the ftp server using the command USER and wait for confirmation (331)
// send password using the command PASS and wait for confirmation that you are logged on the server (230)
// send file:
// use the passive mode: send command PASV
// receive answer with an IP address and a port (227), parse this message.
// connect a second socket(a data socket) with the given configuration
// use the command STOR on the control socket
// send data through the data socket, close data socket.
// leave session using on the control socket the command QUIT.

ftp_t::ftp_t(const char *host_name, const unsigned short server_port)
    : m_server_port(server_port),
      m_server_ip(host_name),
      sock_ctrl(0),
      sock_data(0)
{
  server_status = create_socket(sock_ctrl, m_server_ip.c_str(), m_server_port); // Create connection
}

int ftp_t::create_socket(int &sock, const char *server_ip, const unsigned short server_port)
{
  // Server address
  struct sockaddr_in server_addr;

  // Construct the server address structure
  memset(&server_addr, 0, sizeof(server_addr));       // zero out structure
  server_addr.sin_family = AF_INET;                   // internet address family
  server_addr.sin_addr.s_addr = inet_addr(server_ip); // server IP address
  server_addr.sin_port = htons(server_port);          // server port
  sock = ::socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  
  // Establish the connection to the server
  if (::connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0)
  {
    server_status = -1;
    last_rsp = "Connection Error!";
    return FTP_STATUS::SERVER_CONNECTION_ERROR;
  }
  server_status = 1;
  return FTP_STATUS::SERVER_CONNECTION_SUCCESS;
}


int ftp_t::login(const char *user_name, const char *pass)
{
  char buf_request[255];
  std::string str_rsp;

  // create the control socket
  if (is_connected())
  {
    get_response(sock_ctrl, str_rsp);

    // construct USER request message using command line parameters
    sprintf(buf_request, "USER %s\r\n", user_name);

    // send USER request
    send_request(sock_ctrl, buf_request);

    // receive response
    get_response(sock_ctrl, str_rsp);

    // construct PASS request message using command line parameters
    sprintf(buf_request, "PASS %s\r\n", pass);

    // send PASS request
    send_request(sock_ctrl, buf_request);

    // receive response
    get_response(sock_ctrl, str_rsp);

    // Check if login failed or not
    if (!str_rsp.find("530", 0))
      m_error_code = SERVER_LOGIN_ERROR;
    else
      m_error_code = SERVER_LOGIN_SUCCESS;
    last_rsp = str_rsp;
    return m_error_code;
  }
  else
  {
    exit(1);
  }
}

int ftp_t::logout()
{
  char buf_request[255];

  // construct QUIT request message
  sprintf(buf_request, "QUIT\r\n");

  // send QUIT request (on control socket)
  send_request(sock_ctrl, buf_request);

  // get 'Goodbye' response
  get_response(sock_ctrl, last_rsp);

  close_socket(sock_ctrl);
  return 1;
}

std::vector<std::string> ftp_t::get_file_list()
{
  char buf_request[255];
  std::string str_server_ip;
  unsigned short server_port = 21;

  // enter passive mode: make PASV request
  sprintf(buf_request, "PASV\r\n");

  // send PASV request
  send_request(sock_ctrl, buf_request);

  // receive response
  get_response(sock_ctrl, last_rsp);

  // parse the PASV response
  parse_PASV_response(last_rsp, str_server_ip, server_port);

  // create the data socket
  create_socket(sock_data, str_server_ip.c_str(), server_port);

  // construct NLST (list) request message
  sprintf(buf_request, "NLST\r\n");

  // send NLST request (on control socket)
  send_request(sock_ctrl, buf_request);

  // get response on control socket
  get_response(sock_ctrl, last_rsp);

  // get list on the data socket
  std::vector<std::string> file_list = receive_list(sock_data);

  // get response on control socket
  get_response(sock_ctrl, last_rsp);

  // close data socket
  close_socket(sock_data);
  return file_list;
}

int ftp_t::get_file(const char *file_name)
{
  char buf_request[255];
  std::string str_server_ip;

  unsigned short server_port;

  std::cout << "gettting ";
  sprintf(buf_request, "SIZE %s\r\n", file_name);

  // send SIZE request
  send_request(sock_ctrl, buf_request);

  // get response on control socket
  get_response(sock_ctrl, last_rsp);

  // parse the file size
  std::string str_code = last_rsp.substr(0, 3);
  unsigned long long size_file = 0;
  if ("213" == str_code)
  {
    std::string str_size = last_rsp.substr(4, last_rsp.size() - 2 - 4); // subtract end CRLF plus start of "213 ", 213 SP
    size_file = std::stoull(str_size);

    std::cout << "FILE transfer is " << size_file << " bytes" << std::endl;
  }

  // enter passive mode: make PASV request
  sprintf(buf_request, "PASV\r\n");

  // send PASV request
  send_request(sock_ctrl, buf_request);

  // get response on control socket
  get_response(sock_ctrl, last_rsp);

  // parse the PASV response
  parse_PASV_response(last_rsp, str_server_ip, server_port);

  // create the data socket
  create_socket(sock_data, str_server_ip.c_str(), server_port);

  // construct RETR request message
  sprintf(buf_request, "RETR %s\r\n", file_name);

  // send RETR request on control socket
  send_request(sock_ctrl, buf_request);

  // get response on control socket
  get_response(sock_ctrl, last_rsp);

  // get the file (data socket), save to local file with same name
  receive_all(sock_data, file_name);

  // get response
  get_response(sock_ctrl, last_rsp);

  // close data socket
  close_socket(sock_data);

  return 1;
}

std::vector<std::string>  ftp_t::receive_list(int sock)
{
  int recv_size; 
  const int flags = 0;
  const int size_buf = 255;
  char buf[size_buf];
  std::string str_nlst;
  std::vector<std::string> fm_file_nslt;
  while (1)
  {
    if ((recv_size = recv(sock, buf, size_buf, flags)) == -1)
    {
      std::cout << "recv error: " << strerror(errno) << std::endl;
      exit(1);
    }
    if (recv_size == 0)
    {
      std::cout << "all bytes received " << std::endl;
      break;
    }
    for (int i = 0; i < recv_size; i++)
    {
      str_nlst += buf[i];
    }
  }

  if (!str_nlst.size())
  {
    return fm_file_nslt;
  }

  // parse file list into a vector
  size_t start = 0;
  size_t count = 0;
  for (size_t idx = 0; idx < str_nlst.size() - 1; idx++)
  {
    // detect CRLF
    if (str_nlst.at(idx) == '\r' && str_nlst.at(idx + 1) == '\n')
    {
      count = idx - start;
      std::string str = str_nlst.substr(start, count);
      start = idx + 2;
      fm_file_nslt.push_back(str);
      std::cout << str << std::endl;
    }
  }
  return fm_file_nslt;
}

std::string  ftp_t::get_response(int sock, std::string &str_rep)
{
  int recv_size; // size in bytes received or -1 on error
  const int flags = 0;
  const int size_buf = 1024;
  char buf[size_buf];

  if ((recv_size = recv(sock, buf, size_buf, flags)) == -1)
  {
    std::cout << "recv error: " << strerror(errno) << std::endl;
  }

  std::string str(buf, recv_size);
 
  str_rep = str;
  return str;
}

int ftp_t::send_request(int sock, const char *buf_request)
{
  send_all(sock, (void *)buf_request, strlen(buf_request));
  return 1;
}

///////////////////////////////////////////////////////////////////////////////////////
// ftp_t::parse_PASV_response()
// PASV request asks the server to accept a data connection on a new TCP port selected by the server
// PASV parameters are prohibited
// The server normally accepts PASV with code 227
// Its response is a single line showing the IP address of the server and the TCP port number
// where the server is accepting connections
// RFC 959 failed to specify details of the response format.
// implementation examples
// 227 Entering Passive Mode (h1,h2,h3,h4,p1,p2).
// the TCP port number is p1*256+p2
///////////////////////////////////////////////////////////////////////////////////////

int ftp_t::parse_PASV_response(const std::string &str_rsp, std::string &str_server_ip, unsigned short &server_port)
{
  unsigned int h[4];
  unsigned int p[2];
  char server_ip[100];

  size_t pos = str_rsp.find('(');
  std::string str_ip = str_rsp.substr(pos + 1);
  sscanf(str_ip.c_str(), "%u,%u,%u,%u,%u,%u", &h[0], &h[1], &h[2], &h[3], &p[0], &p[1]);
  server_port = static_cast<unsigned short>(p[0] * 256 + p[1]);
  sprintf(server_ip, "%u.%u.%u.%u", h[0], h[1], h[2], h[3]);
  str_server_ip = server_ip;
  return 1;
}

int ftp_t::close_socket(int sock)
{
  ::close(sock);
  return 1;
}


int ftp_t::receive_all(int sock, const char *file_name)
{
  int recv_size; // size in bytes received or -1 on error
  const int flags = 0;
  const int size_buf = 4096;
  char buf[size_buf];
  FILE *file;

  file = fopen(file_name, "wb");
  while (1)
  {
    if ((recv_size = recv(sock, buf, size_buf, flags)) == -1)
    {
      std::cout << "recv error: " << strerror(errno) << std::endl;
    }
    if (recv_size == 0)
    {
      std::cout << "all bytes received " << std::endl;
      break;
    }
    fwrite(buf, recv_size, 1, file);
  }
  fclose(file);
  return 1;
}


int ftp_t::send_binary(int sock, const char *file_name)
{
  char buffer[1024];
  FILE *fd = fopen(file_name, "rb");
  size_t rret, wret;
  int bytes_read = 0;
  int total_sent = 0;
  int fsize = get_bin_size(file_name);

  while (!feof(fd))
  {
    if ((bytes_read = fread(&buffer, 1, 1024, fd)) > 0)
    {
      total_sent += bytes_read;
      send(sock, buffer, bytes_read, 0);
    }
  }
  return 1;
}
int ftp_t::send_all(int sock, const void *vbuf, int size_buf)
{

  const char *buf = (char *)vbuf; // can't do pointer arithmetic on void*
  int send_size;                  // size in bytes sent or -1 on error
  int size_left;                  // size left to send
  const int flags = 0;

  size_left = size_buf;
  while (size_left > 0)
  {
    if ((send_size = send(sock, buf, size_left, flags)) == -1)
    {
      std::cout << "send error: " << std::endl;
    }
    size_left -= send_size;
    buf += send_size;
  }

  return 1;
}



int ftp_t::send_file(const char *file_name)
{
  char buf_request[255];
  std::string str_server_ip;
  std::string str_rsp;
  unsigned short server_port;


  // enter passive mode: make PASV request
  sprintf(buf_request, "PASV\r\n");

  // send PASV request
  send_request(sock_ctrl, buf_request);

  // get response on control socket
  get_response(sock_ctrl, str_rsp);

  // parse the PASV response
  parse_PASV_response(str_rsp, str_server_ip, server_port);

  // create the data socket
  create_socket(sock_data, str_server_ip.c_str(), server_port);

  // construct RETR request message
  sprintf(buf_request, "STOR %s\r\n", file_name);

  // send RETR request on control socket
  send_request(sock_ctrl, buf_request);

  // get response on control socket
  get_response(sock_ctrl, str_rsp);

  send_binary(sock_data, file_name);

  // close data socket
  close_socket(sock_data);

  return 1;
}

int ftp_t::get_bin_size(const char *filename)
{
  std::ifstream ifile(filename);
  ifile.seekg(0, std::ios_base::end); // seek to end
  // now get current position as length of file
  return ifile.tellg();
}

std::string ftp_t::get_last_sv_response()
{
  return last_rsp;
}

int ftp_t::is_connected()
{
  return server_status;
}