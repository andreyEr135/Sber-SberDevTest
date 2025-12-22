#ifndef configH
#define configH

#include <vector>
#include <list>
#include <string>

#define fOK 0

// Структура параметра конфигурации
struct TParam
{
    std::string* group; // Ссылка на строку в списке групп
    std::string  name;
    std::string  value;
};

typedef std::list<std::string> TGroups;
typedef std::vector<TParam>    TParams;

class TConf
{
private:
    TGroups m_groups;
    TParams m_params;
    int     m_lastError;

public:
    TConf();
    virtual ~TConf();

    // Загрузка файла
    int LoadFile(const char* fileName);
    int LoadFile(const std::string& fileName);

    // Чтение данных
    const std::string GetParamNameById(const std::string& groupName, int paramId);
    const std::string ReadString(const std::string& groupName, const std::string& paramName);
    const int         ReadInt(const std::string& groupName, const std::string& paramName);
    const int         ReadHex(const std::string& groupName, const std::string& paramName);
    const float       ReadFloat(const std::string& groupName, const std::string& paramName);

    // Служебные методы
    void PrintAll();
    const char* GetLastErrorStr();
};

#endif
