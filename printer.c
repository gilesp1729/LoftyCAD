#include "stdafx.h"
#include "LoftyCAD.h"
#include <stdio.h>

#include <winsock.h> 


// Communication with printer.

void
send_to_serial(char* gcode_file)
{
}


void
get_octo_version()
{



}


void
send_to_octo(char* gcode_file)
{
    WSADATA wsaData;

    /* first what are we going to send and where are we going to send it? */
    int portno = 80;
    char* host = "octopi.local";
    char* message_fmt = "GET /api/version?apikey=%s HTTP/1.1\r\n\r\n";
    // TEMP hardcoded
    char* apikey = "ca4e5ff1a119050f53a483cf7e463ec1";
    int err;

    struct hostent* server;
    struct sockaddr_in serv_addr;
    int sockfd, bytes, sent, received, total;
    char message[1024], response[4096];

    /* Initialise Winsock */
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != NO_ERROR)
        return;

    /* fill in the parameters */
    sprintf_s(message, 1024, message_fmt, apikey);
    Log(message);

    /* create the socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        Log("ERROR opening socket\r\n");

    /* lookup the ip address */
    server = gethostbyname(host);
    if (server == NULL) 
        Log("ERROR no such host\r\n");

    /* fill in the structure */
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno);
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);

    /* connect the socket */
    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
        Log("ERROR connecting\r\n");

    /* send the request */
    total = strlen(message);
    sent = 0;
    do 
    {
        bytes = send(sockfd, message + sent, total - sent, 0);
        if (bytes < 0)
            Log("ERROR writing message to socket\r\n");
        if (bytes <= 0)
            break;
        sent += bytes;
    } while (sent < total);

    /* receive the response */
    memset(response, 0, sizeof(response));
    total = sizeof(response) - 1;
    received = 0;
    do 
    {
        // Is anything there to be read? Check with a timeout to avoid blocking
        fd_set readfds = { 1, sockfd };
        TIMEVAL tv = { 0, 100000 };

        if (select(1, &readfds, NULL, NULL, &tv) == 0)
            break;

        // Read the data
        bytes = recv(sockfd, response + received, total - received, 0);
        if (bytes < 0)
            Log("ERROR reading response from socket\r\n");
        if (bytes <= 0)
            break;
        received += bytes;
    } while (received < total);

    if (received == total)
        Log("ERROR storing complete response from socket\r\n");

    /* close the socket */
    closesocket(sockfd);

    /* shutdown */
    WSACleanup();

    /* process response */
    Log(response);
}
