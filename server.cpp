/*
* requires gcc 4.8 or above
* make && ./server PORT DATA_FILE
*/

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <cstring>
#include <sys/wait.h>
#include <signal.h>
#include <stdio.h>
#include <strings.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

const uint64_t m1  = 0x5555555555555555; //binary: 0101...
const uint64_t m2  = 0x3333333333333333; //binary: 00110011..
const uint64_t m4  = 0x0f0f0f0f0f0f0f0f; //binary:  4 zeros,  4 ones ...
const uint64_t m8  = 0x00ff00ff00ff00ff; //binary:  8 zeros,  8 ones ...
const uint64_t m16 = 0x0000ffff0000ffff; //binary: 16 zeros, 16 ones ...
const uint64_t m32 = 0x00000000ffffffff; //binary: 32 zeros, 32 ones
const uint64_t hff = 0xffffffffffffffff; //binary: all ones
const uint64_t h01 = 0x0101010101010101; //the sum of 256 to the power of 0,1,2,3...

int main (int argc, char* argv[]) {
  std::string file = "/tmp/data";
  int port = 11600;
  if (argc >= 2) port = atoi(argv[1]);
  if (argc == 3) file = argv[2];
  std::cout << "loading file: " << file << "..." << std::endl;
  std::string line;
  std::ifstream myfile (file);
  std::cout << "loading file: done" << std::endl;

  if (myfile.is_open()) {
    int cnt = 0, bad_cnt = 0, bit_count = 0;
    uint64_t inp = 0;
    uint64_t num, x;

    int N = 200000000, total_count = 0;
    uint64_t *lines = (uint64_t *) malloc(sizeof(uint64_t) * N);
    uint64_t *pl = lines;
    printf("start");
    while( getline (myfile, line) ) {
      uint64_t num;
      sscanf(line.c_str(), "%llu", pl++);
      total_count++;
    }

    std::cout << "total count: " << total_count << std::endl;
    std::cout << "bad count: " << bad_cnt << std::endl;
    myfile.close();

    int sockfd, newsockfd, clilen;
    char buffer[256];
    struct sockaddr_in serv_addr, cli_addr;
    int n;

    /* First call to socket() function */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
      perror("ERROR opening socket");
      exit(1);
    }
    /* Initialize socket structure */
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    /* Now bind the host address using bind() call.*/
    if (bind(sockfd, (struct sockaddr *) &serv_addr,
          sizeof(serv_addr)) < 0)
    {
      perror("ERROR on binding");
      exit(1);
    }
    /* Now start listening for the clients, here
     * process will go in sleep mode and will wait
     * for the incoming connection
     */
    listen(sockfd, 5);
    clilen = sizeof(cli_addr);
    std::cout << "listening on port: " << port << std::endl;
    while (1) {
      newsockfd = accept(sockfd,
          (struct sockaddr *) &cli_addr, (socklen_t *)&clilen);
      if (newsockfd < 0) {
        perror("ERROR on accept");
        exit(1);
      }
      // don't care about childs deaths, kernel can kill them so they don't become zombies
      signal(SIGCHLD, SIG_IGN);
      /* Create child process */
      int pid = fork();
      if (pid < 0) {
        perror("ERROR on fork");
        exit(1);
      }
      if (pid == 0) {
        /* This is the client process */
        close(sockfd);

        int n;
        char buffer[256];

        bzero(buffer, 256);

        n = read(newsockfd, buffer, 255);
        if (n < 0) {
          perror("ERROR reading from socket");
          exit(1);
        }
        // std::string string_buffer = buffer;
        std::istringstream ss(buffer);
        std::string token;
        int pieces = 0, threshold = 0, how_many_needed = 0;
        while(getline(ss, token, ',')) {
          pieces++;
          std::cout << "||" << token << std::endl;
          if (pieces == 1) inp = stoul(token, NULL, 0);
          if (pieces == 2) threshold = stoul(token, NULL, 0);
          if (pieces == 3) how_many_needed = stoul(token, NULL, 0);
        }
        printf("Here IS the message: %s\n", buffer);
        if (pieces != 3) {
          perror("Didn't receive all the pieces");
          exit(1);
        }

        // do I have a off-by-one error? big freaking deal, so what? wanna fight about it? @soheil
        int found_count = 0;
        for (int j = 0; j < total_count; j++) {
          num = lines[j];
          x = inp ^ num;

          x -= (x >> 1) & m1;             //put count of each 2 bits into those 2 bits
          x = (x & m2) + ((x >> 2) & m2); //put count of each 4 bits into those 4 bits
          x = (x + (x >> 4)) & m4;        //put count of each 8 bits into those 8 bits
          bit_count = (x * h01)>>56;      //returns left 8 bits of x + (x<<8) + (x<<16) + (x<<24) + ...

          if (bit_count < threshold) {
            found_count++;
            std::cout << j << ":" << num << std::endl;
            char result[256];
            sprintf(result, "%llu\n", num);
            n = write(newsockfd, &result, strlen(result));
            if (n < 0) {
              perror("ERROR writing to socket");
              exit(1);
            }
            if (found_count >= how_many_needed) break;
          }
        }
        exit(0);
      } else {
        close(newsockfd);
      }
    } /* end of while */

  }

  else std::cout << "Unable to open file" << std::endl;
  return 0;
}
