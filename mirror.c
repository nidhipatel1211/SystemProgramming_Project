#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/sendfile.h>
#include <sys/wait.h>
#include <limits.h>

#define MIRROR_PORT 9090
#define MAX_COMMAND 1024
#define MAX_PENDING 5
#define MAX_BUFFER 1024
#define MAX_PATH 4096
#define MAX_FILENAME 256
#define BACKLOG 10
#define MAX_PATH 4096
#define MAX_FILENAME 256
#define BUFFER_SIZE 1024

void tar_dir(const char *directory, const char *output_file, int size1, int size2);
void adding_files_to_tar(const char *filename, const char *tar_file);
void send_file(int client_socket, char *filename);
void tar_dir_by_date(char *dir_path, char *output_file, char *date1, char *date2);
void search_file(int client_socket, const char *filename, const char *dir_path);
void search_temp_file(int client_socket, const char *filename, const char *dir_path);
void check_file(int client_socket, const char *filename);
void check_tar_file(int client_socket, const char *filename);
int match_extension(char *filename, char **extensions, int num_extensions);
int tar_dir_with_extensions(char *dirname, char *output_filename, char **extensions, int num_extensions);
void process_client(int client_socket);
int tar_dir_with_files(char *dir, char *output_file, char **filenames, int num_filenames)
{
    // Create a tar file with the given directory and files
    int ret = 0;
    char command[MAX_COMMAND];
    snprintf(command, MAX_COMMAND, "tar -czf %s -C %s ", output_file, dir);
    for (int i = 0; i < num_filenames; i++)
    {
        strcat(command, filenames[i]);
        strcat(command, " ");
    }
    ret = system(command);
    return ret;
}

int main() {

    int mirror_fd, new_socket, valread;
    struct sockaddr_in mirror_addr;
    int opt = 1;
    int addrlen = sizeof(mirror_addr);
    char buffer[1024] = {0};
    char *hello = "Connected to mirror";
    // Creating socket file descriptor for mirror
    if ((mirror_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    // Forcefully attaching socket to the mirror port
    if (setsockopt(mirror_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    mirror_addr.sin_family = AF_INET;
    mirror_addr.sin_addr.s_addr = INADDR_ANY;
    mirror_addr.sin_port = htons(MIRROR_PORT);
    // Forcefully attaching socket to the mirror port
    if (bind(mirror_fd, (struct sockaddr *)&mirror_addr, sizeof(mirror_addr))<0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(mirror_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    while (1) {
        printf("Mirror server is waiting for a client...\n");
        if ((new_socket = accept(mirror_fd, (struct sockaddr *)&mirror_addr, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }
        printf("Connection established with main server.\n");
        send(new_socket, hello, strlen(hello), 0);
        printf("Connected message sent to main server.\n");
        close(new_socket);

        // fork child process to service client request
        if (fork() == 0) {
            close(mirror_fd);
            process_client(new_socket);
            exit(0);  // ensure child process exits normally
        } else {
            int status;
            waitpid(-1, &status, 0);  // wait for any child process to terminate
            if (WIFEXITED(status)) {
                printf("Client disconnected with quit\n");
            }
        }
    }
    return 0;
}

void search_temp_file(int client_socket, const char *filename, const char *dir_path)
{
    DIR *dir;
    struct dirent *entry;
    struct stat filestat;
    char path[1024];
    int found = 0;
    if ((dir = opendir(dir_path)) == NULL)
    {
        perror("opendir error");
        exit(1);
    }
    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }
        snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name);
        if (lstat(path, &filestat) < 0)
        {
            perror("lstat error");
            exit(1);
        }
        if (S_ISDIR(filestat.st_mode))
        {
            search_file(client_socket, filename, path);
        }
        else if (S_ISREG(filestat.st_mode))
        {
            if (strcmp(entry->d_name, filename) == 0)
            {
                char file_details[1024];
                sprintf(file_details, "%ld",
                        filestat.st_size);
                printf("\nPrinting Size: %ld\n", filestat.st_size);
                send(client_socket, file_details, strlen(file_details), 0);
                found = 1;
                break;
            }
        }
    }
    if (!found && strcmp(dir_path, getenv("HOME")) == 0)
    {
        char not_found[1024];
        sprintf(not_found, "File not found\n");
        send(client_socket, not_found, strlen(not_found), 0);
    }
    closedir(dir);
}

void check_file(int client_socket, const char *filename)
{
    search_file(client_socket, filename, getenv("HOME"));
}

void check_tar_file(int client_socket, const char *filename)
{
    search_temp_file(client_socket, filename, getenv("HOME"));
}

int match_extension(char *filename, char **extensions, int num_extensions)
{
    char *ext = strrchr(filename, '.');
    if (ext == NULL)
    {
        return 0;
    }

    ext++; // skip the dot
    for (int i = 0; i < num_extensions; i++)
    {
        if (strcmp(ext, extensions[i]) == 0)
        {
            return 1;
        }
    }

    return 0;
}

int tar_dir_with_extensions(char *dirname, char *output_filename, char **extensions, int num_extensions)
{
    DIR *dir;
    struct dirent *ent;
    struct stat st;
    char filepath[MAX_PATH];
    char command[MAX_BUFFER];
    int ret;

    // create tar command
    snprintf(command, MAX_BUFFER, "tar -czf %s", output_filename);

    dir = opendir(dirname);
    if (dir == NULL)
    {
        perror("opendir");
        return 1;
    }

    while ((ent = readdir(dir)) != NULL)
    {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
        {
            continue;
        }

        snprintf(filepath, MAX_PATH, "%s/%s", dirname, ent->d_name);
        if (lstat(filepath, &st) == -1)
        {
            perror("lstat");
            continue;
        }

        if (S_ISDIR(st.st_mode))
        {
            ret = tar_dir_with_extensions(filepath, output_filename, extensions, num_extensions);
            if (ret != 0)
            {
                return ret;
            }
        }
        else if (S_ISREG(st.st_mode))
        {
            if (match_extension(ent->d_name, extensions, num_extensions))
            {
                snprintf(filepath, MAX_PATH, "%s/%s", dirname, ent->d_name);
                snprintf(command + strlen(command), MAX_BUFFER - strlen(command), " %s", filepath);
            }
        }
    }

    closedir(dir);

    // run tar command
    ret = system(command);
    if (ret != 0)
    {
        perror("system");
        return ret;
    }

    return 0;
}

void process_client(int client_socket)
{
    char buffer[MAX_BUFFER];
    int bytes_received;
    int size1, size2;
    while (1)
    {
        // wait for client to send a command
        //Findfile START

        bytes_received = recv(client_socket, buffer, MAX_BUFFER, 0);
        if (bytes_received == -1)
        {
            perror("recv");
            exit(1);
        }
        // terminate string
        buffer[bytes_received] = '\0';
        // check for quit command
        if (strcmp(buffer, "quit") == 0)
        {
            close(client_socket);
            exit(0);
        }
        // check for findfile command
        if (strncmp(buffer, "findfile", 8) == 0)
        {
            // extract filename from command
            char *filename = buffer + 9; // skip over "findfile "
            check_file(client_socket, filename);
        }
        //Findfile END

        //sgetfiles START

        else if (strncmp(buffer, "sgetfiles", 9) == 0)
        {
            char *arg1 = strtok(buffer + 9, " ");
            char *arg2 = strtok(NULL, " ");

            size1 = atoi(arg1);
            size2 = atoi(arg2);

            char output_file[MAX_FILENAME];
            snprintf(output_file, MAX_FILENAME, "temp.tar.gz");

            tar_dir(getenv("HOME"), output_file, size1, size2);

            printf("Output file created: %s\n", output_file);
            char *filename = "temp.tar.gz";
            check_tar_file(client_socket, filename);
            send_file(client_socket, output_file);
            char ackbuffer[1024] = {0};
            int ackread = read(client_socket, ackbuffer, 1024);
            printf("%s\n", ackbuffer);
            if (strncmp(ackbuffer, "Success", 7) == 0)
            {
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
        //sgetfiles END

        //dgetfiles START

        else if (strncmp(buffer, "dgetfiles", 9) == 0)
        {
            char *date1 = strtok(buffer + 9, " ");
            char *date2 = strtok(NULL, " ");
            // Extract date1, date2, and -u option from the command

            // Create a tar file containing the files with date of creation >=date1 and <=date2
            char output_file[MAX_FILENAME];
            snprintf(output_file, MAX_FILENAME, "temp.tar.gz");
            tar_dir_by_date(getenv("HOME"), output_file, date1, date2);
            printf("Output file created: %s\n", output_file);
            char *filename = "temp.tar.gz";
            check_tar_file(client_socket, filename);
            send_file(client_socket, output_file);

            // Delete the file using the remove() function
            char ackbuffer[1024] = {0};
            int ackread = read(client_socket, ackbuffer, 1024);

            printf("%s\n", ackbuffer);
            if (strncmp(ackbuffer, "Success2", 8) == 0)
            {
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
        //dgetfiles END
        else if (strncmp(buffer, "getfiles", 8) == 0)
        {
            // Extract the filenames from the command
            char *filenames[6];
            int num_filenames = 0;
            char *token = strtok(buffer + 9, " ");
            while (token != NULL && num_filenames < 6)
            {
                filenames[num_filenames++] = token;
                token = strtok(NULL, " ");
            }

            if (num_filenames == 0)
            {
                char *result = "No files specified";
                send(client_socket, result, strlen(result), 0);
            }
            else
            {
                char output_file[MAX_FILENAME];
                snprintf(output_file, MAX_FILENAME, "temp.tar.gz");

                int ret = tar_dir_with_files(getenv("HOME"), output_file, filenames, num_filenames);

                if (ret == 0)
                {
                    printf("Output file created: %s\n", output_file);
                    char *filename = "temp.tar.gz";
                    check_tar_file(client_socket, filename);
                    send_file(client_socket, output_file);

                    // Delete the file using the remove() function
                    char ackbuffer[1024] = {0};
                    int ackread = read(client_socket, ackbuffer, 1024);

                    printf("%s\n", ackbuffer);
                    if (strncmp(ackbuffer, "Success", 8) == 0)
                    {
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
                else
                {
                    char *result = "No file found";
                    send(client_socket, result, strlen(result), 0);
                }
            }
        }
        //getfiles END
        
        else
        {
            // unknown command
            char *result = "Unknown command";
            send(client_socket, result, strlen(result), 0);
        }
    }
}

void search_file(int client_socket, const char *filename, const char *dir_path)
{
    DIR *dir;
    struct dirent *entry;
    struct stat filestat;
    char path[1024];
    int found = 0;
    if ((dir = opendir(dir_path)) == NULL)
    {
        perror("opendir error");
        exit(1);
    }
    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }
        snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name);
        if (lstat(path, &filestat) < 0)
        {
            perror("lstat error");
            exit(1);
        }
        if (S_ISDIR(filestat.st_mode))
        {
            search_file(client_socket, filename, path);
        }
        else if (S_ISREG(filestat.st_mode))
        {
            if (strcmp(entry->d_name, filename) == 0)
            {
                char file_details[1024];
                sprintf(file_details, "Filename: %s\nSize: %ld bytes\nDate created: %s",
                        filename, filestat.st_size, ctime(&filestat.st_mtime));
                send(client_socket, file_details, strlen(file_details), 0);
                found = 1;
                break;
            }
        }
    }
    if (!found && strcmp(dir_path, getenv("HOME")) == 0)
    {
        char not_found[1024];
        sprintf(not_found, "File not found\n");
        send(client_socket, not_found, strlen(not_found), 0);
    }
    closedir(dir);
}

void send_file(int client_socket, char *filename)
{
    int file = open(filename, O_RDONLY);
    if (file == -1)
    {
        printf("Error: cannot open file %s\n", filename);
        return;
    }

    struct stat file_stat;
    if (fstat(file, &file_stat) < 0)
    {
        printf("Error: cannot get file size.\n");
        close(file);
        return;
    }

    off_t offset = 0;
    int sent_bytes = sendfile(client_socket, file, &offset, file_stat.st_size);
    printf("bytes sent: %d", sent_bytes);
    if (sent_bytes < 0)
    {
        printf("Error: failed to send file %s.\n", filename);
        close(file);
        return;
    }

    close(file);

    printf("File %s sent successfully.\n", filename);
}
void tar_dir(const char *directory, const char *output_file, int size1, int size2)
{
    DIR *dir;
    struct dirent *entry;
    char path[MAX_PATH];

    dir = opendir(directory);
    if (dir == NULL)
    {
        printf("Error opening directory %s\n", directory);
        exit(1);
    }

    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        snprintf(path, MAX_PATH, "%s/%s", directory, entry->d_name);

        struct stat st;
        if (lstat(path, &st) < 0)
        {
            printf("Error getting information for file %s\n", path);
            continue;
        }

        if (S_ISDIR(st.st_mode))
        {
            tar_dir(path, output_file, size1, size2);
        }
        else if (S_ISREG(st.st_mode))
        {
            if (st.st_size >= size1 && st.st_size <= size2)
            {
                adding_files_to_tar(path, output_file);
            }
        }
    }

    closedir(dir);
}
void tar_dir_by_date(char *dir_path, char *output_file, char *date1, char *date2)
{
    // Create a temporary file to store the list of files
    char temp_file[MAX_FILENAME];
    snprintf(temp_file, MAX_FILENAME, "/tmp/files_%d", getpid());

    // Find the files with date of creation between date1 and date2 and write their paths to the temporary file
    char command[MAX_BUFFER];
    snprintf(command, MAX_BUFFER, "find %s -type f -newermt \"%s\" ! -newermt \"%s\" > %s", dir_path, date1, date2, temp_file);
    system(command);

    // Create a tar file containing the files listed in the temporary file
    snprintf(command, MAX_BUFFER, "tar -czf %s -T %s", output_file, temp_file);
    system(command);

    // Delete the temporary file
    snprintf(command, MAX_BUFFER, "rm %s", temp_file);
    system(command);
}
void adding_files_to_tar(const char *filename, const char *tar_file)
{
    pid_t pid = fork();
    if (pid < 0)
    {
        printf("Error forking process to add file to tar archive\n");
        return;
    }
    else if (pid == 0)
    {
        execlp("tar", "tar", "-rf", tar_file, filename, NULL);
        exit(1);
    }
    else
    {
        waitpid(pid, NULL, 0);
    }
}