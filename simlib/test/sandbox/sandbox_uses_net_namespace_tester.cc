#include <arpa/inet.h>
#include <cstring>
#include <netdb.h>
#include <netinet/in.h>
#include <simlib/throw_assert.hh>
#include <sys/socket.h>

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    throw_assert(sock >= 0);

    sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("1.1.1.1");
    server_addr.sin_port = htons(80);
    throw_assert(
        connect(sock, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) == -1 &&
        errno == ENETUNREACH
    );
}
