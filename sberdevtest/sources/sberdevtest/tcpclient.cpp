#include <string>
#include <cstring>      // Для memset
#include <sys/socket.h> // Для socket, connect, send, recv
#include <netdb.h>      // Для getaddrinfo, freeaddrinfo, gai_strerror
#include <unistd.h>     // Для close
#include <errno.h>      // Для errno, strerror

#include "debugsystem.h"

#include "main.h"

#include "tcpclient.h"

using namespace std;

//-------------------------------------------------------------------------------------------

TcpClient* Tcp;

#undef  UNIT
#define UNIT  g_DebugSystem.units[ TCP_UNIT ]
#include "log_macros.def"

//-------------------------------------------------------------------------------------------

TcpClient::TcpClient() : socket_fd(-1)
{FUNCTION_TRACE
    std::string ip = g_DebugSystem.conf->ReadString( "Server", "ip" );
    std::string port = g_DebugSystem.conf->ReadString( "Server", "port" );
    connected = connectToServer(ip, port);
}

//-------------------------------------------------------------------------------------------

TcpClient::~TcpClient() {
    FUNCTION_TRACE
    disconnect();
    connected = false;
}

//-------------------------------------------------------------------------------------------

// Метод для подключения к серверу
// @param host: Имя хоста или IP-адрес сервера (например, "localhost", "192.168.1.1")
// @param port: Номер порта сервера (например, "80", "8080")
// @return: true в случае успешного подключения, false при ошибке
bool TcpClient::connectToServer(const std::string& host, const std::string& port)
{FUNCTION_TRACE
    struct addrinfo hints, *servinfo, *p;
    int rv;

    // Если сокет уже открыт, сначала закрываем его
    if (socket_fd != -1) {
        close(socket_fd);
        socket_fd = -1;
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;     // Использовать IPv4 или IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP потоковый сокет

    // Получаем информацию об адресе сервера
    if ((rv = getaddrinfo(host.c_str(), port.c_str(), &hints, &servinfo)) != 0) {
        last_error = "getaddrinfo: " + std::string(gai_strerror(rv));
        return false;
    }

    // Перебираем все полученные адреса и пытаемся подключиться
    for(p = servinfo; p != NULL; p = p->ai_next) {
        // Создаем сокет
        if ((socket_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            last_error = "socket: " + std::string(strerror(errno));
            continue; // Пробуем следующий адрес
        }

        // Пытаемся подключиться
        if (connect(socket_fd, p->ai_addr, p->ai_addrlen) == -1) {
            close(socket_fd); // Закрываем сокет при неудаче и пробуем следующий адрес
            socket_fd = -1;
            last_error = "connect: " + std::string(strerror(errno));
            continue;
        }

        break; // Если подключение удалось, выходим из цикла
    }

    freeaddrinfo(servinfo); // Освобождаем память, выделенную getaddrinfo

    if (p == NULL) {
        // Не удалось подключиться ни к одному адресу
        if (last_error.empty()) { // Если ошибок не было, но и подключения нет
             last_error = "Не удалось подключиться к серверу: неизвестная ошибка.";
        }
        return false;
    }

    last_error = ""; // Очищаем ошибку в случае успеха
    return true;
}

// Метод для закрытия сокета
void TcpClient::disconnect()
{FUNCTION_TRACE
    if (socket_fd != -1) {
        close(socket_fd);
        socket_fd = -1;
        connected = false;
    }
}

// Возвращает дескриптор открытого сокета (-1, если сокет не открыт)
int TcpClient::getSocketDescriptor() {
    FUNCTION_TRACE
    return socket_fd;
}

// Дополнительные методы для отправки/получения данных (для примера)
ssize_t TcpClient::sendData(const void* data, size_t len)
{FUNCTION_TRACE
    if (socket_fd == -1) {
        last_error = "Сокет не открыт для отправки данных.";
        return -1;
    }
    ssize_t bytes_sent = send(socket_fd, data, len, 0);
    if (bytes_sent == -1) {
        last_error = "send: " + std::string(strerror(errno));
    }
    return bytes_sent;
}

ssize_t TcpClient::recvData(void* buffer, size_t len)
{FUNCTION_TRACE
    if (socket_fd == -1) {
        last_error = "Сокет не открыт для получения данных.";
        return -1;
    }
    ssize_t bytes_received = recv(socket_fd, buffer, len, 0);
    if (bytes_received == -1) {
        last_error = "recv: " + std::string(strerror(errno));
    } else if (bytes_received == 0) {
        last_error = "Соединение закрыто удаленным хостом.";
    }
    return bytes_received;
}

//-------------------------------------------------------------------------------------------

