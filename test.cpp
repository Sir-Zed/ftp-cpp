#include <string>
#include <iostream>
#include <stdio.h>
#include <string.h>
#include "lib/ftp.hh"

void usage()
{
  std::cout << "usage: ftp_client -s SERVER -u USER -p PASS" << std::endl;
  std::cout << "-s SERVER: fully qualified ftp server name" << std::endl;
  std::cout << "-u USER: user name" << std::endl;
  std::cout << "-p PASS: password" << std::endl;
  exit(0);
}

int main(int argc, char *argv[])
{
  char *host_name = NULL;       // server name
  const char *user_name = NULL; // user name
  const char *pass = NULL;      // password

  if (argc != 7)
  {
    usage();
  }

  for (int i = 1; i < argc; i++)
  {
    if (argv[i][0] == '-')
    {
      switch (argv[i][1])
      {
      case 's':
        host_name = argv[i + 1];
        // std::cout << host_name << std::endl;
        i++;
        break;
      case 'u':
        user_name = argv[i + 1];
        i++;
        break;
      case 'p':
        pass = argv[i + 1];
        i++;
        break;
      default:
        usage();
      }
    }
    else
    {
      usage();
    }
  }
  ftp_t ftp("192.168.1.94", 21);
 

  if (ftp.is_connected() >= 1)
  {
    if (ftp.login("meti", "1111") == ftp_t::FTP_STATUS::SERVER_LOGIN_SUCCESS)
    {
      std::cout << "Logged in!";
      ftp.get_file("lp.pdf");
      std::cout << "Recv" << std::endl;
      ftp.send_file("tst.tgz");
      std::cout << "Sent" << std::endl;
      ftp.logout();


    }
    else
    {
      std::cout << ftp.get_last_sv_response();
    }
  }
  else
  {
    std::cout << ftp.get_last_sv_response() << std::endl;
  }
  //std::cin >> host_name;


  return 0;
}