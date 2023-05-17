#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <poll.h>

int main()
{
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == -1)
    {
        perror("socket() failed");
        return 1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(9000);

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)))
    {
        perror("bind() failed");
        return 1;
    }

    if (listen(listener, 5))
    {
        perror("listen() failed");
        return 1;
    }

    struct pollfd fds[1024];
    int nfds = 1; // Số lượng file descriptor trong mảng fds

    fds[0].fd = listener;
    fds[0].events = POLLIN;

    char buf[256];

    int users[64];      // Mang socket client da dang nhap
    char *user_ids[64]; // Mang id client da dang nhap
    int num_users = 0;  // So luong client da dang nhap

    while (1)
    {
        int ret = poll(fds, nfds, 5000); // Chờ sự kiện xảy ra trong vòng 5 giây

        if (ret < 0)
        {
            perror("poll() failed");
            return 1;
        }

        if (ret == 0)
        {
            printf("Timed out!!!\n");
            continue;
        }

        if (fds[0].revents & POLLIN) // Có sự kiện kết nối mới
        {
            int client = accept(listener, NULL, NULL);
            if (client < 0)
            {
                perror("accept() failed");
                return 1;
            }

            fds[nfds].fd = client;
            fds[nfds].events = POLLIN;
            nfds++;

            printf("New client connected: %d\n", client);
        }

        for (int i = 1; i < nfds; i++)
        {
            if (fds[i].revents & POLLIN) // Có dữ liệu từ client i
            {
                int ret = recv(fds[i].fd, buf, sizeof(buf), 0);
                if (ret <= 0)
                {
                    close(fds[i].fd);

                    // Xoá client khỏi mảng fds
                    fds[i] = fds[nfds - 1];
                    nfds--;

                    // Kiểm tra nếu client đã đăng nhập thì xóa khỏi danh sách
                    for (int j = 0; j < num_users; j++)
                    {
                        if (users[j] == fds[i].fd)
                        {
                            free(user_ids[j]);
                            // Xoá client khỏi danh sách
                            users[j] = users[num_users - 1];
                            user_ids[j] = user_ids[num_users - 1];
                            num_users--;
                            break;
                        }
                    }
                }
                else
                {
                    buf[ret] = '\0';
                    printf("Received from %d: %s\n", fds[i].fd, buf);

                    int client = fds[i].fd;

                    // Kiểm tra trạng thái đăng nhập
                    int j = 0;
                    for (; j < num_users; j++)
                    {
                        if (users[j] == client)
                        {
                            break;
                        }
                    }

                    if (j == num_users) // Chưa đăng nhập
                    {
                        // Xử lý cú pháp lệnh đăng nhập
                        char cmd[32], id[32], tmp[32];
                        ret = sscanf(buf, "%s%s%s", cmd, id, tmp);
                        if (ret == 2 && strcmp(cmd, "client_id:") == 0)
                        {
                            char *msg = "Dung cu phap. Hay nhap tin nhan de chuyen tiep.\n";
                            send(client, msg, strlen(msg), 0);

                            // Lưu vào mảng user
                            users[num_users] = client;
                            user_ids[num_users] = (char *)malloc(strlen(id) + 1);
                            strcpy(user_ids[num_users], id);
                            num_users++;
                        }
                        else
                        {
                            char *msg = "Sai cu phap. Hay nhap lai.\n";
                            send(client, msg, strlen(msg), 0);
                        }
                    }
                    else // Đã đăng nhập
                    {
                        // id: user_ids[j]
                        // data: buf

                        char sendbuf[256];

                        strcpy(sendbuf, user_ids[j]);
                        strcat(sendbuf, ": ");
                        strcat(sendbuf, buf);

                        // Chuyển tiếp dữ liệu cho các user
                        for (int k = 0; k < num_users; k++)
                        {
                            if (users[k] != client)
                            {
                                send(users[k], sendbuf, strlen(sendbuf), 0);
                            }
                        }
                    }
                }
            }
        }
    }

    close(listener);

    return 0;
}