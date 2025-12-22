#ifndef tcpclientH
#define tcpclientH
//-------------------------------------------------------------------------------------------

#include "debugsystem.h"

//-------------------------------------------------------------------------------------------

class TcpClient
{
private:
    int socket_fd;          // Дескриптор сокета
    std::string last_error; // Сообщение о последней ошибке
    bool connected;

public:
    // Конструктор: инициализирует дескриптор сокета как невалидный
    TcpClient();

    // Деструктор: закрывает сокет, если он был открыт
    ~TcpClient();

    // Метод для подключения к серверу
    // @param host: Имя хоста или IP-адрес сервера (например, "localhost", "192.168.1.1")
    // @param port: Номер порта сервера (например, "80", "8080")
    // @return: true в случае успешного подключения, false при ошибке
    bool connectToServer(const std::string& host, const std::string& port);
    bool isConnected() { return connected; };

    // Метод для закрытия сокета
    void disconnect();

    // Возвращает дескриптор открытого сокета (-1, если сокет не открыт)
    int getSocketDescriptor();

    // Возвращает последнее сообщение об ошибке
    const std::string& getLastError() const {
        return last_error;
    }

    // Дополнительные методы для отправки/получения данных (для примера)
    ssize_t sendData(const void* data, size_t len);

    ssize_t recvData(void* buffer, size_t len);
};
//-------------------------------------------------------------------------------------------

extern TcpClient* Tcp;

//-------------------------------------------------------------------------------------------
#endif
