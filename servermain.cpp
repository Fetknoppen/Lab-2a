#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <iostream>
/* You will to add includes here */
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <string>
#include <string.h>
// Included to get the support library
#include <calcLib.h>

// Enable if you want debugging to be printed, see examble below.
// Alternative, pass argument during compilation '-DDEBUG'
#define DEBUG
#define BACKLOG 5
#define YES 1

using namespace std;

void sigchld_handler(int s)
{
  (void)s; // quiet unused variable warning

  // waitpid() might overwrite errno, so we save and restore it:
  int saved_errno = errno;

  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;

  errno = saved_errno;
}
// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
  if (sa->sa_family == AF_INET)
  {
    return &(((struct sockaddr_in *)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
  initCalcLib();
  if (argc != 2)
  {
    printf("Invalid input.\n");
    exit(1);
  }
  /*
    Read first input, assumes <ip>:<port> syntax, convert into one string (Desthost) and one integer (port). 
     Atm, works only on dotted notation, i.e. IPv4 and DNS. IPv6 does not work if its using ':'. 
  */
  char delim[] = ":";
  char *Desthost = strtok(argv[1], delim);
  char *Destport = strtok(NULL, delim);
  // *Desthost now points to a sting holding whatever came before the delimiter, ':'.
  // *Dstport points to whatever string came after the delimiter.
  if (Desthost == NULL || Destport == NULL)
  {
    printf("Invalid input.\n");
    exit(1);
  }

  /* Do magic */
  int port = atoi(Destport);
#ifdef DEBUG
  printf("Host %s, and port %d.\n", Desthost, port);
#endif

  char *protocols = "TEXT TCP\n\n";

  int sockfd, new_fd; //Litsen on sock_fd, new conection on new_fd
  struct addrinfo hints, *serverinfo, *p;
  struct sockaddr_storage their_addr; //connectors address information
  socklen_t sin_size;
  struct sigaction sa;
  int yes = 1;
  char s[INET6_ADDRSTRLEN];
  int rv;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE; // use my IP

  rv = getaddrinfo(NULL, to_string(port).c_str(), &hints, &serverinfo);
  if (rv != 0)
  {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    exit(1);
  }

  //loop through all the results and bind to the first we can
  for (p = serverinfo; p != NULL; p->ai_next)
  {
    //Create socket
    sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (sockfd == -1)
    {
      printf("Could not make socket. Trying again.\n");
      continue;
    }
    //setsockoptions
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
    {
      printf("setsockopt.\n");
      exit(1);
    }
    //bind
    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
    {
      close(sockfd);
      printf("Could not bind.\n");
      continue;
    }
    break;
  }

  freeaddrinfo(serverinfo); //Done with thus struct

  if (p == NULL)
  {
    printf("Could not bind.\n");
    exit(1);
  }
  if (listen(sockfd, BACKLOG) == -1)
  {
    printf("Cant litsen.\n");
    exit(1);
  }

  sa.sa_handler = sigchld_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGCHLD, &sa, NULL) == -1)
  {
    printf("sigaction");
  }

  printf("Waiting for connections.\n");

  char buf[10000];
  int MAXSZ = sizeof(buf) - 1;

  int childCount = 0;
  int readSize;
  char command[10];
  double f1, f2, fRes;
  int i1, i2, iRes;

  while (true) //main accept loop
  {
    sin_size = sizeof(their_addr);
    new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
    if (new_fd == -1)
    {
      printf("Accept error.\n");
      continue;
    }
    inet_ntop(their_addr.ss_family,
              get_in_addr((struct sockaddr *)&their_addr),
              s, sizeof(s));
    printf("server: Connection %d from %s\n", childCount, s);

    struct sockaddr_in *local_sin = (struct sockaddr_in *)&their_addr;
    if (send(new_fd, "TEXT TCP 1.0\n\n", 14, 0) == -1)
    {
      printf("Send error.\n");
      close(new_fd);
      continue;
    }
    memset(&buf, 0, sizeof(buf));
    readSize = recv(new_fd, &buf, MAXSZ, 0);
    if (readSize == 0)
    {
      printf("Child [%d] died.\n", childCount);
      close(new_fd);
      break;
    }
    std::cout<<buf;
    printf("Child[%d] (%s:%d): recv(%d) .\n", childCount, s, ntohs(local_sin->sin_port), readSize);
    //if client accepts the protocols
    if (strcmp(buf,"OK\n")==0)
    {
      cout<<"got and ok from client\n";
      //get random calculation and two random numbers
      string operation = randomType();
      if (operation.at(0) == 'f')
      {
        cout<<"float"<<endl;
        //Float
        f1 = randomFloat();
        f2 = randomFloat();
        if (operation == "fadd")
        {
          fRes = f1 + f2;
        }
        else if (operation == "fsub")
        {
          fRes = f1 - f2;
        }
        else if (operation == "fmul")
        {
          fRes = f1 * f2;
        }
        else if (operation == "fdiv")
        {
          fRes = f1 / f2;
        }
        
        string msg = operation+" "+to_string(f1)+" "+to_string(f2)+"\n";
        cout<<"sending: "<<msg;
        if (send(new_fd, msg.c_str(), sizeof(msg), 0) == -1)
        {
          printf("Send error.\n");
          close(new_fd);
          continue;
        }
      }
      else
      {
        //int
        cout<<"int"<<endl;
        i1 = randomInt();
        i2 = randomInt();
        if (operation == "add")
        {
          iRes = i1 + i1;
        }
        else if (operation == "fsub")
        {
          iRes = i1 - i1;
        }
        else if (operation == "mul")
        {
          iRes = i1 * i1;
        }
        else if (operation == "div")
        {
          iRes = i1 / i1;
        }
        string msg = operation+" "+to_string(i1)+" "+to_string(i2)+"\n";
        cout<<"sending: "<<msg;
        if (send(new_fd, msg.c_str(), sizeof(msg), 0) == -1)
        {
          printf("Send error.\n");
          close(new_fd);
          continue;
        }
      }
      //send task
    }
  }

  return 0;
}
