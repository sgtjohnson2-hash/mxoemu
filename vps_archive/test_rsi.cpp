#include <iostream>
#include <iomanip>
#include "RsiData.h"

int main() {
    RsiDataMale rsi;
    rsi["Sex"] = 0;
    rsi["Body"] = 0;
    rsi["Hat"] = 0;
    rsi["Face"] = 0;
    rsi["Shirt"] = 2;
    rsi["Coat"] = 10;
    rsi["Pants"] = 1;
    rsi["Shoes"] = 6;
    rsi["Gloves"] = 6;
    rsi["Glasses"] = 4;
    rsi["Hair"] = 0;
    rsi["FacialDetail"] = 0;
    rsi["ShirtColor"] = 41;
    rsi["PantsColor"] = 16;
    rsi["CoatColor"] = 0;
    rsi["ShoeColor"] = 0;
    rsi["GlassesColor"] = 15;
    rsi["HairColor"] = 0;
    rsi["SkinTone"] = 0;
    rsi["Tattoo"] = 0;
    rsi["FacialDetailColor"] = 0;

    unsigned char buf[64] = {0};
    unsigned int len = rsi.ToBytes(buf, sizeof(buf));
    std::cout << "LEN: " << len << " BYTES: ";
    for (unsigned int i = 0; i < len; ++i) {
        std::cout << "0x" << std::hex << std::setw(2) << std::setfill('0') << (int)buf[i] << ", ";
    }
    std::cout << std::endl;
    return 0;
}
