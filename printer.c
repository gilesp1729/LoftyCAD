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
init_comms(void)
{
    WSADATA wsaData;

    /* Initialise Winsock */
    WSAStartup(MAKEWORD(2, 2), &wsaData);
}

void
close_comms(void)
{
    /* shutdown */
    WSACleanup();
}

// Connect to an Octoprint server.
int
connect_to_socket()
{
    int sockfd;
    int portno = 80;
    struct hostent* server;
    struct sockaddr_in serv_addr;

    /* create the socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        Log("ERROR opening socket\r\n");
        return -1;
    }

    /* lookup the ip address */
    server = gethostbyname(octoprint_server);
    if (server == NULL)
    {
        Log("ERROR no such host\r\n");
        return -1;
    }

    /* fill in the structure */
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno);
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);

    /* connect the socket */
    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
    {
        Log("ERROR connecting\r\n");
        return -1;
    }

    return sockfd;
}

BOOL
get_octo_version(char *buf, int buflen)
{
    char* message_fmt = "GET /api/version?apikey=%s HTTP/1.1\r\n\r\n";
    int sockfd, bytes, sent, received, total;
    char message[1024], response[4096];
    char* brace, * octo, * quote;

    // Connect to the server
    sockfd = connect_to_socket();
    if (sockfd < 0)
        return FALSE;

    /* fill in the parameters */
    sprintf_s(message, 1024, message_fmt, octoprint_apikey);
    Log(message);

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
        // Is anything there to be read? Check with a 100ms timeout to avoid blocking
        // on the last call.
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
    {
        Log("ERROR storing complete response from socket\r\n");
        closesocket(sockfd);
        return FALSE;
    }

    // make sure it's null terminated.
    response[received] = '\0';

    /* close the socket */
    closesocket(sockfd);

    /* process response */
    Log(response);

    // Parse out the OctoPrint version, skipping all the bumph in the response.
    brace = strchr(response, '{');
    octo = strstr(brace, "OctoPrint");
    if (octo == NULL)
        return FALSE;
    strcpy_s(buf, buflen, octo);
    quote = strchr(buf, '\"');
    *quote = '\0';
    return TRUE;
}


/* Sending a file to OctoPrint for printing.

The message looks as follows:

    POST /api/files/local HTTP/1.1
    Host: example.com
    X-Api-Key: abcdef...
    Content-Type: multipart/form-data; boundary=----WebKitFormBoundaryDeC2E3iWbTv1PwMC

    ------WebKitFormBoundaryDeC2E3iWbTv1PwMC
    Content-Disposition: form-data; name="file"; filename="file_to_send.gcode"
    Content-Type: application/octet-stream

    M109 T0 S220.000000
    T0
    G21
    G90
    ...
    ------WebKitFormBoundaryDeC2E3iWbTv1PwMC--

With optional extras:

    Content-Disposition: form-data; name="select"

    true
    ------WebKitFormBoundaryDeC2E3iWbTv1PwMC
    Content-Disposition: form-data; name="print"

    true
    ------WebKitFormBoundaryDeC2E3iWbTv1PwMC--

The response is expected as follows:

    HTTP/1.1 200 OK
    Content-Type: application/json
    Location: http://example.com/api/files/local/file_to_send.gcode

    {
      "files": {
        "local": {
          "name": "file_to_send",
          "origin": "local",
          "refs": {
            "resource": "http://example.com/api/files/local/file_to_send.gcode",
            "download": "http://example.com/downloads/files/local/file_to_send.gcode"
          }
        }
      },
      "done": true
    }

*/

void
send_to_octoprint(char* gcode_file)
{
#if 0
    char* message = "POST /api/version HTTP/1.1\r\n\r\n";
    int sockfd, bytes, sent, received, total;
    char response[4096];
    char* brace, * octo, * quote;

    // Connect to the server
    sockfd = connect_to_socket();
    if (sockfd < 0)
        return FALSE;

    /* fill in the parameters */
    sprintf_s(message, 1024, message_fmt, octoprint_apikey);
    Log(message);






#endif
}
