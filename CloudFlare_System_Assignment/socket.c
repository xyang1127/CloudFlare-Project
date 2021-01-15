#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <getopt.h>

#define BUFFER_SIZE 1024
typedef enum
{
    false,
    true
} bool;

int startsWith(char *a, char *b)
{
    if (strncmp(a, b, strlen(b)) == 0)
        return 1;
    return 0;
}

char *welcomemessage = "This is a command line tool that sends HTTP GET request to user specified URLs.\nIt is implemented using socket directly without taking advantage of external library to send HTTP request\nIt also provide the functionality to measure the executioin of the command\nYou can refer to \"./socket -h\" command to check out how to use it.\n";
char *helpmessage = "Usage: ./socket [option...] -u <url>: The tool will make an HTTP request to the URL and print the response directly to the console.\n option: -p: a positive integer indicating the number of times requests are made to the url, it will print out some measurement about how fast the command runs\n";

// return the number of bytes received.
long sendrequest(char *url, int measure)
{
    int sockfd, portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    long rcvdBytes = 0;

    // assume using "http" protocol
    portno = 80;

    // create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        fprintf(stderr, "cannot assign socket\n");
        exit(1);
    }

    // fill in sockaddr_in struct
    // parse URL into host, port, path. example url: https://my-worker.xyang42.workers.dev
    // first get rid of protocol if there exists one
    char *tmp;
    if (strlen(url) > 8 && startsWith(url, "https://"))
    {
        int l = strlen(url);
        tmp = calloc(l - 8 + 1, sizeof(char));
        strncpy(tmp, url + 8, l - 8);
    }
    else if (strlen(url) > 7 && startsWith(url, "http://"))
    {
        int l = strlen(url);
        tmp = calloc(l - 7 + 1, sizeof(char));
        strncpy(tmp, url + 7, l - 7);
    }
    else
        tmp = url;
    // printf("tmp extracted is %s\n", tmp);

    // then extract host name and path
    char *first_slash = strchr(tmp, '/');
    char *host, *path;
    if (first_slash == NULL)
    {
        path = "/";
        host = tmp;
    }
    else
    {
        path = first_slash;
        host = calloc(first_slash - tmp + 1, sizeof(char));
        strncpy(host, tmp, first_slash - tmp);
    }
    // printf("host extracted is %s\n", host);
    // printf("path extracted is %s\n", path);

    server = gethostbyname(host);
    if (server == NULL)
    {
        fprintf(stderr, "cannot find such host\n");
        exit(0);
    }
    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);

    // connect the socket to the server
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        fprintf(stderr, "cannot connect to the server\n");
        exit(1);
    }

    char *requestMessage = calloc(1024, sizeof(char));
    sprintf(requestMessage, "GET %s HTTP/1.0\r\nHost: %s\r\n\r\n", path, host);
    // printf("packet msg is: \n%s", requestMessage);
    send(sockfd, requestMessage, strlen(requestMessage), 0);
    // printf("write operation successes\n");

    char buffer[BUFFER_SIZE];
    bzero(buffer, BUFFER_SIZE);
    int curBytes = 0;
    while ((curBytes = read(sockfd, buffer, BUFFER_SIZE - 1)) != 0)
    {
        rcvdBytes += (long)curBytes;
        if (!measure)
            fprintf(stdout, "%s", buffer);
        bzero(buffer, BUFFER_SIZE);
    }
    // printf("read operation successes\n");

    // shut down
    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);
    // fprintf(stdout, "success!\n");
    return rcvdBytes;
}

void measure(char *url, int n)
{
    double time[n];
    double fastest, slowest;
    long smallestResponse = 0;
    long largestResponse = 0;
    clock_t start, end;
    double cpu_time_used;
    long curBytes;

    for (int i = 0; i < n; i++)
    {
        start = clock();
        curBytes = sendrequest(url, 1);
        end = clock();
        cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
        time[i] = cpu_time_used;
        if (i == 0)
        {
            fastest = cpu_time_used;
            slowest = cpu_time_used;
            smallestResponse = curBytes;
            largestResponse = curBytes;
        }
        else
        {
            if (smallestResponse > curBytes)
                smallestResponse = curBytes;
            if (largestResponse < curBytes)
                largestResponse = curBytes;
            if (fastest > cpu_time_used)
                fastest = cpu_time_used;
            if (slowest < cpu_time_used)
                slowest = cpu_time_used;
        }
    }
    double mean, total = 0;
    for (int i = 0; i < n; i++)
        total += time[i];
    mean = total / (double)n;

    printf("%d time(s) of HTTP GET request is made to %s. Below are some data about this execution\n", n, url);
    printf("Number of requests made: %d\n", n);
    printf("The fastest time: %f s\n", fastest);
    printf("The slowest time: %f s\n", slowest);
    printf("The mean time: %f s\n", mean);
    printf("The size in bytes of the smallest response: %ld bytes\n", smallestResponse);
    printf("The size in bytes of the largest response: %ld bytes\n", largestResponse);
}

int main(int argc, char **argv)
{
    int option_index = 0;
    char *url = "";
    int profile = -1;
    while ((option_index = getopt(argc, argv, "u:p:h")) != -1)
    {
        switch (option_index)
        {
        case 'u':
            url = optarg;
            break;
        case 'p':
            profile = atoi(optarg);
            break;
        case 'h':
            printf("%s\n", helpmessage);
            return 0;
            break;
        default:
            printf("%s\n", welcomemessage);
            return 0;
            break;
        }
    }
    if (profile > 0 && strlen(url) > 0)
        measure(url, profile);
    else if (strlen(url) > 0)
        sendrequest(url, 0);
    else if (profile > 0)
        printf("you need to provide the url option\n");
    else
        printf("invalid command: check -h option for how to use this cmd tool");

    return 0;
}