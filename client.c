#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>

#define SERVER_IP "127.0.0.1" // Replace with the actual server IP address
#define SERVER_PORT 8000      // Replace with the actual server port number
#define BUFFER_SIZE 1024

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

int main()
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        error("ERROR: Failed to create socket.\n");
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0)
    {
        error("ERROR: Invalid server IP address.\n");
    }

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        error("ERROR: Failed to connect to the server.\n");
    }

    char command[256];
    char filename[256];
    while (1)
    {
        printf("Enter a command: ");
        fgets(command, 256, stdin);
        command[strlen(command) - 1] = '\0'; // Remove the newline character
 ////////=============Findfile START=============================================================//////////////////
        if (strncmp(command, "findfile ", 9) == 0)
        {

            snprintf(filename, sizeof(filename), "%s", command + 9); // Extract the filename from the command

            char message[512];
            snprintf(message, sizeof(message), "findfile %s", filename);

            if (send(sockfd, message, strlen(message), 0) == -1)
            {
                error("ERROR: Failed to send command to the server.\n");
            }

            char response[1024];
            int bytes_received = recv(sockfd, response, sizeof(response), 0);
            if (bytes_received == -1)
            {
                error("ERROR: Failed to receive response from the server.\n");
            }
            else if (bytes_received == 0)
            {
                printf("Connection closed by the server.\n");
                break;
            }
            else
            {
                response[bytes_received] = '\0'; // Add the null terminator
                if (strcmp(response, "File not found") == 0)
                {
                    printf("File not found\n");
                }
                else
                {
                    printf("%s\n", response);
                }
            }
        }
 ////////=============Findfile END=============================================================//////////////////
  ////////=============sgetfiles START=============================================================//////////////////
        else if (strncmp(command, "sgetfiles", 9) == 0)
        {

            int num = 0;
            char message[512];
            int unzip_flag = 0;
            char *token;
            token = strtok(command, "-");
            while (token != NULL)
            {
                if (strcmp(token, "u") == 0)
                {
                    unzip_flag = 1;
                    break;

                }
                token = strtok(NULL, " ");
            }
            snprintf(message, sizeof(message), "%s", command);
            // printf("Message:: %s\n",message);

            if (send(sockfd, message, strlen(message), 0) == -1)
            {
                error("ERROR: Failed to send command to the server.\n");
            }

            char response[1024];
            int bytes_received = recv(sockfd, response, sizeof(response), 0);
            // response[bytes_received] = '\0';
            printf("recent: %s\n", response);

            if (bytes_received == -1)
            {
                error("ERROR: Failed to receive response from the server.\n");
            }
            else if (bytes_received == 0)
            {
                printf("Connection closed by the server.\n");
                break;
            }
            else
            {
                if (strcmp(response, "File not found") == 0)
                {
                    printf("File not found\n");
                    continue;
                }
                num = atoi(response);
                printf("Number of bytes to receive: %d\n", num);
            }

            char *fileresponse = (char *)malloc(num);
            int filebytes_received = 0;
            int total_bytes_received = 0;

            while ((filebytes_received = recv(sockfd, fileresponse + total_bytes_received, num - total_bytes_received, 0)) > 0)
            {
                total_bytes_received += filebytes_received;
                printf("Total bytes received so far: %d\n", total_bytes_received);
                if (total_bytes_received >= num)
                {
                    break;
                }
            }

            if (filebytes_received == -1)
            {
                error("ERROR: Failed to receive response from the server.\n");
            }

            FILE *file = fopen("temp.tar.gz", "wb");
            if (!file)
            {
                error("ERROR: Failed to open file.\n");
            }
            fwrite(fileresponse, 1, num, file);
            fclose(file);

            printf("File received successfully.\n");

            char *ack = "Success";
            send(sockfd, ack, strlen(ack), 0);

            free(fileresponse);
            

            if (unzip_flag)
            {
                // Unzip the received file using system call
                int result = system("tar -xvf temp.tar.gz");
                if (result != 0)
                {
                    error("ERROR: Failed to unzip file.\n");
                }
                printf("File unzipped successfully.\n");
                char *temp = "temp.tar.gz";

                // Delete the file using the remove() function
                if (remove(temp) == 0)
                {
                    printf("File %s deleted successfully\n", temp);
                }
                else
                {
                    fprintf(stderr, "Error: unable to delete file %s\n", temp);
                }
            }
        }
  ////////=============sgetfiles END=============================================================//////////////////
  ////////============dgetfiles START=============================================================//////////////////

        else if (strncmp(command, "dgetfiles", 9) == 0)
        {
            int num = 0;
            char message[512];
            snprintf(message, sizeof(message), "%s", command);

            if (send(sockfd, message, strlen(message), 0) == -1)
            {
                error("ERROR: Failed to send command to the server.\n");
            }
            char response[1024];
            int bytes_received = recv(sockfd, response, sizeof(response), 0);
            if (bytes_received == -1)
            {
                error("ERROR: Failed to receive response from the server.\n");
            }
            else if (bytes_received == 0)
            {
                printf("Connection closed by the server.\n");
                break;
            }
            else
            {
                // response[bytes_received] = '\0'; // Add the null terminator
                if (strcmp(response, "File not found") == 0)
                {
                    printf("File not found\n");
                }
                else
                {
                    printf("%s\n", response);
                    num = atoi(response);
                    printf("%d\n", num);
                }
            }
            char fileresponse[num];
            int filebytes_received = 0;
            int total_bytes_received = 0;
            while ((filebytes_received = recv(sockfd, fileresponse + total_bytes_received, sizeof(fileresponse) - total_bytes_received - 1, 0)) > 0)
            {
                printf("bytes received: %d", filebytes_received);
                total_bytes_received += filebytes_received;
                if (total_bytes_received >= num)
                {
                    break;
                }
            }
            if (filebytes_received == -1)
            {
                error("ERROR: Failed to receive response from the server.\n");
            }
            fileresponse[total_bytes_received] = '\0';

            FILE *file = fopen("temp.tar.gz", "wb");
            if (!file)
            {
                error("ERROR: Failed to open file.\n");
            }
            fwrite(fileresponse, 1, total_bytes_received, file);
            fclose(file);

            printf("File received successfully.\n");
            char *ack = "Success2";
            send(sockfd, ack, strlen(ack), 0);
        }
          ////////============dgetfiles END=============================================================//////////////////
                    ////////============getfiles START=============================================================//////////////////

        else if (strncmp(command, "getfiles", 8) == 0)
        {

           int num = 0;
            char message[512];
            int unzip_flag = 0;
            char *token;
            token = strtok(command, "-");
            while (token != NULL)
            {
                if (strcmp(token, "u") == 0)
                {
                    unzip_flag = 1;
                    break;

                }
                token = strtok(NULL, " ");
            }
            snprintf(message, sizeof(message), "%s", command);
            // printf("Message:: %s\n",message);

            if (send(sockfd, message, strlen(message), 0) == -1)
            {
                error("ERROR: Failed to send command to the server.\n");
            }

            char response[1024];
            int bytes_received = recv(sockfd, response, sizeof(response), 0);
            // response[bytes_received] = '\0';
            printf("recent: %s\n", response);

            if (bytes_received == -1)
            {
                error("ERROR: Failed to receive response from the server.\n");
            }
            else if (bytes_received == 0)
            {
                printf("Connection closed by the server.\n");
                break;
            }
            else
            {
                if (strcmp(response, "File not found") == 0)
                {
                    printf("File not found\n");
                    continue;
                }
                num = atoi(response);
                printf("Number of bytes to receive: %d\n", num);
            }

            char *fileresponse = (char *)malloc(num);
            int filebytes_received = 0;
            int total_bytes_received = 0;

            while ((filebytes_received = recv(sockfd, fileresponse + total_bytes_received, num - total_bytes_received, 0)) > 0)
            {
                total_bytes_received += filebytes_received;
                printf("Total bytes received so far: %d\n", total_bytes_received);
                if (total_bytes_received >= num)
                {
                    break;
                }
            }

            if (filebytes_received == -1)
            {
                error("ERROR: Failed to receive response from the server.\n");
            }

            FILE *file = fopen("temp.tar.gz", "wb");
            if (!file)
            {
                error("ERROR: Failed to open file.\n");
            }
            fwrite(fileresponse, 1, num, file);
            fclose(file);

            printf("File received successfully.\n");

            char *ack = "Success";
            send(sockfd, ack, strlen(ack), 0);

            free(fileresponse);
            

            if (unzip_flag)
            {
                // Unzip the received file using system call
                int result = system("tar -xvf temp.tar.gz");
                if (result != 0)
                {
                    error("ERROR: Failed to unzip file.\n");
                }
                printf("File unzipped successfully.\n");
                char *temp = "temp.tar.gz";

                // Delete the file using the remove() function
                if (remove(temp) == 0)
                {
                    printf("File %s deleted successfully\n", temp);
                }
                else
                {
                    fprintf(stderr, "Error: unable to delete file %s\n", temp);
                }
            }
        }
                            ////////============getfiles END=============================================================//////////////////

        
        else
        {
            printf("Invalid command.\n");
            continue;
        }
    }

    close(sockfd);
    return 0;
}