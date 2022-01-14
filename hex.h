#ifndef HEX_H
#define HEX_H
#include <string>
#include <sstream>
#include <iomanip>
#include <cctype>

class Hex
{
private:
    int last_address;
    

public:
    Hex();
    std::string stringToHEx(const std::string &data, int type=0);
    std::string intToHEX(int data, int cells = 2);
    ~Hex();
};
Hex::Hex() : last_address(0)
{
}

std::string Hex::intToHEX(int data, int cells)
{
    std::stringstream dataHEX;
    dataHEX << std::hex << std::setw(cells) << std::setfill('0') << data;
    return dataHEX.str();
}

std::string Hex::stringToHEx(const std::string &data, int type)
{
    //размер закодированной информмации В int и в HEX
    int dataSize = data.size();
    unsigned char sum = dataSize;
    std::string RECLEN = intToHEX(dataSize);
    //адрес
    sum += last_address;
    std::string LOAD_OFFSET = intToHEX(last_address, 4);
    last_address += dataSize;
    //тип
    sum += type;
    std::string RECTYP = intToHEX(type);
    //закодировал информмацию
    std::string DATA;
    for (int i = 0; i < dataSize; i++)
    {
        int intChar = (int)data[i];
        sum += intChar;
        DATA += intToHEX(intChar);
    };
    
    // контрольная сумма
    std::string CHKSUM = intToHEX(256 - (int)sum);
    
    //вывод
    std::string buf(':' + RECLEN + LOAD_OFFSET + RECTYP + DATA + CHKSUM);
    for (int i = 0; i < buf.size(); i++)
    {
        buf[i] = std::toupper(buf[i]);
    }
    return buf;
}

Hex::~Hex()
{
}

#endif