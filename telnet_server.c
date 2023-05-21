#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>


#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024
#define USER_DATABASE_FILE "user_database.txt"

typedef struct {
    int socket;
    char *username;
} Client;

void handleClientLogin(Client *client) {
    char buffer[BUFFER_SIZE];
    char *username, *password;
    
    // Yêu cầu client gửi user và pass
    send(client->socket, "Enter username: ", strlen("Enter username: "), 0);
    recv(client->socket, buffer, BUFFER_SIZE, 0);
    buffer[strlen(buffer) - 1] = '\0'; // Xóa ký tự '\n' cuối dòng
    username = strdup(buffer);
    
    send(client->socket, "Enter password: ", strlen("Enter password: "), 0);
    recv(client->socket, buffer, BUFFER_SIZE, 0);
    buffer[strlen(buffer) - 1] = '\0'; // Xóa ký tự '\n' cuối dòng
    password = strdup(buffer);
    
    // So sánh với cơ sở dữ liệu người dùng
    FILE *file = fopen(USER_DATABASE_FILE, "r");
    if (file != NULL) {
        char line[BUFFER_SIZE];
        int validUser = 0;
        
        while (fgets(line, BUFFER_SIZE, file) != NULL) {
            line[strlen(line) - 1] = '\0'; // Xóa ký tự '\n' cuối dòng
            
            char *storedUsername = strtok(line, " ");
            char *storedPassword = strtok(NULL, " ");
            
            if (strcmp(username, storedUsername) == 0 && strcmp(password, storedPassword) == 0) {
                validUser = 1;
                break;
            }
        }
        
        fclose(file);
        
        if (!validUser) {
            send(client->socket, "Invalid username or password. Closing connection.\n", strlen("Invalid username or password. Closing connection.\n"), 0);
            close(client->socket);
            free(client->username);
            return;
        }
    } else {
        send(client->socket, "Error reading user database. Closing connection.\n", strlen("Error reading user database. Closing connection.\n"), 0);
        close(client->socket);
        free(client->username);
        return;
    }
    
    client->username = username;
    send(client->socket, "Login successful.\n", strlen("Login successful.\n"), 0);
}

void handleClientCommand(Client *client) {
    char buffer[BUFFER_SIZE];
    
    // Đợi lệnh từ client
    send(client->socket, "Enter a command: ", strlen("Enter a command: "), 0);
    recv(client->socket, buffer, BUFFER_SIZE, 0);
    buffer[strlen(buffer) - 1] = '\0'; // Xóa ký tự '\n' cuối dòng
    
    // Thực hiện lệnh và trả kết quả cho client
    char command[BUFFER_SIZE + 10];
    snprintf(command, BUFFER_SIZE + 10, "%s > out.txt", buffer);
    system(command);
    
    FILE *file = fopen("out.txt", "r");
    if (file != NULL) {
        char line[BUFFER_SIZE];
        
        while (fgets(line, BUFFER_SIZE, file) != NULL) {
            send(client->socket, line, strlen(line), 0);
        }
        
        fclose(file);
    } else {
        send(client->socket, "Error executing command.\n", strlen("Error executing command.\n"), 0);
    }
}

int main() {
    char buffer[BUFFER_SIZE];
    int serverSocket, clientSocket, maxSocket, activity, i, valread, sd;
    struct sockaddr_in address;
    socklen_t addrlen=sizeof(address);
    fd_set readfds;
    int clientSockets[MAX_CLIENTS];
    Client clients[MAX_CLIENTS];
    
    // Khởi tạo mảng clientSockets và clients
    for (i = 0; i < MAX_CLIENTS; i++) {
        clientSockets[i] = 0;
        clients[i].socket = 0;
        clients[i].username = NULL;
    }
    
    // Tạo socket
    if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Failed to create socket");
        exit(EXIT_FAILURE);
    }
    
    // Thiết lập địa chỉ và cổng
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8888);
    
    // Liên kết socket với địa chỉ và cổng
    if (bind(serverSocket, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Failed to bind");
        exit(EXIT_FAILURE);
    }
    
    // Lắng nghe kết nối từ client
    if (listen(serverSocket, 3) < 0) {
        perror("Failed to listen");
        exit(EXIT_FAILURE);
    }
    
    // Chấp nhận kết nối và xử lý client
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(serverSocket, &readfds);
        maxSocket = serverSocket;
        
        // Thêm các client sockets vào set readfds
        for (i = 0; i < MAX_CLIENTS; i++) {
            sd = clientSockets[i];
            if (sd > 0) {
                FD_SET(sd, &readfds);
            }
            if (sd > maxSocket) {
                maxSocket = sd;
            }
        }
        
        // Sử dụng hàm select để xử lý nhiều kết nối client
        activity = select(maxSocket + 1, &readfds, NULL, NULL, NULL);
        if ((activity < 0) && (errno != EINTR)) {
            perror("Failed to select");
            exit(EXIT_FAILURE);
        }
        
        // Kiểm tra kết nối mới từ client
        if (FD_ISSET(serverSocket, &readfds)) {
            if ((clientSocket = accept(serverSocket, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
                perror("Failed to accept");
                exit(EXIT_FAILURE);
            }
            
            // Thêm client socket vào mảng
            for (i = 0; i < MAX_CLIENTS; i++) {
                if (clientSockets[i] == 0) {
                    clientSockets[i] = clientSocket;
                    clients[i].socket = clientSocket;
                    clients[i].username = NULL;
                    break;
                }
            }
        }
        
        // Xử lý dữ liệu từ các client sockets
        for (i = 0; i < MAX_CLIENTS; i++) {
            sd = clientSockets[i];
            
            if (FD_ISSET(sd, &readfds)) {
                // Kiểm tra nếu kết nối client đã đóng
                if ((valread = recv(sd, buffer, BUFFER_SIZE, 0)) == 0) {
                    // Ngắt kết nối và xoá client socket
                    getpeername(sd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
                    printf("Client disconnected: %s:%d\n",inet_ntoa(address.sin_addr), ntohs(address.sin_port));
                    close(sd);
                    clientSockets[i] = 0;
                    
                    // Giải phóng bộ nhớ
                    if (clients[i].username != NULL) {
                        free(clients[i].username);
                    }
                } else {
                    // Xử lý đăng nhập hoặc lệnh từ client
                    if (clients[i].username == NULL) {
                        handleClientLogin(&clients[i]);
                    } else {
                        handleClientCommand(&clients[i]);
                    }
                }
            }
        }
    }
    
    return 0;
}