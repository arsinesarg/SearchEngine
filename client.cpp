#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <cstring>

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { std::cerr << "socket() failed\n"; return 1; }

    sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port   = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "connect() failed\n"; return 1;
    }

    std::string query;
    std::cout << "Search: ";
    std::getline(std::cin, query);
    query += "\n";

    if (send(sock, query.c_str(), query.size(), 0) < 0) {
        std::cerr << "send() failed\n"; return 1;
    }

    // Read full response until server closes connection
    std::string response;
    char buffer[1024];
    ssize_t n;
    while ((n = read(sock, buffer, sizeof(buffer))) > 0)
        response.append(buffer, n);

    if (n < 0) std::cerr << "read() failed\n";

    std::cout << "\nResults:\n" << response;

    close(sock);
    return 0;
}
