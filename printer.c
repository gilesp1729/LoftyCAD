#include "stdafx.h"
#include "LoftyCAD.h"
#include <stdio.h>

#include <winsock.h> 


// Communication with printer.

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

// Send a G-code file to the serial port.
void
send_to_serial(char* gcode_file)
{
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

// Send a message string to the socket.
BOOL
send_to_socket(int sockfd, char* message, int total)
{
    int sent, bytes;

    sent = 0;
    do
    {
        bytes = send(sockfd, message + sent, total - sent, 0);
        if (bytes < 0)
        {
            int err = WSAGetLastError();
            Log("ERROR writing message to socket\r\n");
        }
        if (bytes <= 0)
            break;
        sent += bytes;
    } while (sent < total);

    return sent == total;
}

BOOL
receive_from_socket(int sockfd, char* response, int resp_len)
{
    int total, bytes, received;

    memset(response, 0, resp_len);
    total = resp_len - 1;
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

    // make sure it's null terminated.
    if (received < total)
        response[received] = '\0';

    return received < total;
}

BOOL
get_octo_version(char *buf, int buflen)
{
    char* message_fmt = "GET /api/version?apikey=%s HTTP/1.1\r\n\r\n";
    int sockfd, bytes;
    char message[1024], response[4096];
    char* brace, * octo, * quote;

    // Connect to the server
    sockfd = connect_to_socket();
    if (sockfd < 0)
        return FALSE;

    /* fill in the parameters */
    bytes = sprintf_s(message, 1024, message_fmt, octoprint_apikey);
    Log(message);

    /* send the request */
    send_to_socket(sockfd, message, bytes);

    /* receive the response */
    if (!receive_from_socket(sockfd, response, sizeof(response)))
    {
        Log("ERROR storing complete response from socket\r\n");
        closesocket(sockfd);
        return FALSE;
    }

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
    Content-Length: nnnnn

    ------WebKitFormBoundaryDeC2E3iWbTv1PwMC
    Content-Disposition: form-data; name="file"; filename="file_to_send.gcode"
    Content-Type: application/octet-stream

    M109 T0 S220.000000
    T0
    G21
    G90
    ...
    ------WebKitFormBoundaryDeC2E3iWbTv1PwMC--

(Content-Length is the byte count from the first byte of the first boundary sequence,
up to and including the two trailing hyphens of the last boundary sequence, including \r\n endings.
The Content-Length is required, even though it is not mentioned in the Octo doco.)

With optional extras:

    Content-Disposition: form-data; name="select"

    true
    ------WebKitFormBoundaryDeC2E3iWbTv1PwMC
    Content-Disposition: form-data; name="print"

    true
    ------WebKitFormBoundaryDeC2E3iWbTv1PwMC--
(note that only the final boundary has "--" appended!)

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
send_to_octoprint(char* gcode_file, char *destination)
{
    char* message_fmt = "POST /api/files/%s HTTP/1.1\r\nX-Api-Key: %s\r\n";
    char* content_type_fmt = "Content-Type: multipart/form-data; boundary=%s\r\n";
    char* content_length_fmt = "Content-Length: %d\r\n\r\n";
    char* content_disp_fmt = "Content-Disposition: form-data; name=\"file\"; filename=\"%s\"\r\n";
    char* boundary = "----WebKitFormBoundaryDeC2E3iWbTv1PwMC";
    char message[1024], first_boundary[256], last_boundary[256], content_type[256], content_disp[256], content_length[256];
    int sockfd, bytes, type_bytes, disp_bytes, length_bytes, file_bytes, b1_bytes, b2_bytes;
    FILE* hf;
    char response[4096];

    // Connect to the server
    sockfd = connect_to_socket();
    if (sockfd < 0)
        return;

    // Fill in the parameters. Destination is either "local" or "sdcard"
    bytes = sprintf_s(message, 1024, message_fmt, destination, octoprint_apikey);
    Log(message);
    send_to_socket(sockfd, message, bytes);

    // Start the multipart form
    type_bytes = sprintf_s(content_type, 256, content_type_fmt, boundary);
    Log(content_type);
    send_to_socket(sockfd, content_type, type_bytes);

    // From here we have to count bytes for the Content-Length. Build up the message
    // parts first before sending any of them.
    b1_bytes = sprintf_s(first_boundary, 256, "--%s\r\n", boundary);
    disp_bytes = sprintf_s(content_disp, 256, content_disp_fmt, "file.gcode"); // TEMP gcode_file); - may have to remove dir etc.
    type_bytes = sprintf_s(content_type, 256, "Content-Type: application/octet-stream\r\n\r\n");
    b2_bytes = sprintf_s(last_boundary, 256, "--%s--\r\n", boundary);
    fopen_s(&hf, gcode_file, "rb");
    fseek(hf, 0, SEEK_END);
    file_bytes = ftell(hf);
    fclose(hf);

    // Send the content length header now we know its size
    length_bytes = sprintf_s(content_length, 256, content_length_fmt,
        b1_bytes + disp_bytes + type_bytes + file_bytes + b2_bytes);
    Log(content_length);
    send_to_socket(sockfd, content_length, length_bytes);

    // Send the headers
    Log(first_boundary);
    send_to_socket(sockfd, first_boundary, b1_bytes);
    Log(content_disp);
    send_to_socket(sockfd, content_disp, disp_bytes);
    Log(content_type);
    send_to_socket(sockfd, content_type, type_bytes);

    // Send the file data
    Log("Sending G-code data... ");
    fopen_s(&hf, gcode_file, "rb");
    while (1)
    {
        bytes = fread_s(message, 1024, 1, 1024, hf);
        if (bytes == 0)
            break;
        if (!send_to_socket(sockfd, message, bytes))
            break;
    }

    fclose(hf);
    Log("sent.\r\n");

    // Finish with a final boundary
    Log(last_boundary);
    send_to_socket(sockfd, last_boundary, b2_bytes);

    // Read back the response and log it to the debug log
    receive_from_socket(sockfd, response, sizeof(response));

    /* close the socket */
    closesocket(sockfd);

    // Log the response
    Log(response);
}
