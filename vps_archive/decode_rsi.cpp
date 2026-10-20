#include <iostream>
#include <iomanip>
#include "RsiData.h"

int main() {
    const unsigned char defaultRsiValues[] = {0x00,0x0C,0x71,0x48,0x18,0x0C,0xE2,0x00,0x23,0x00,0xB0,0x00,0x40,0x00,0x00};
    RsiDataMale rsi;
    rsi.FromBytes(defaultRsiValues, sizeof(defaultRsiValues));
    std::cout << "Sex: " << (int)rsi["Sex"] << std::endl;
    std::cout << "Body: " << (int)rsi["Body"] << std::endl;
    std::cout << "Hat: " << (int)rsi["Hat"] << std::endl;
    std::cout << "Face: " << (int)rsi["Face"] << std::endl;
    std::cout << "Shirt: " << (int)rsi["Shirt"] << std::endl;
    std::cout << "Coat: " << (int)rsi["Coat"] << std::endl;
    std::cout << "Pants: " << (int)rsi["Pants"] << std::endl;
    std::cout << "Shoes: " << (int)rsi["Shoes"] << std::endl;
    std::cout << "Gloves: " << (int)rsi["Gloves"] << std::endl;
    std::cout << "Glasses: " << (int)rsi["Glasses"] << std::endl;
    std::cout << "Hair: " << (int)rsi["Hair"] << std::endl;
    std::cout << "ShirtColor: " << (int)rsi["ShirtColor"] << std::endl;
    std::cout << "PantsColor: " << (int)rsi["PantsColor"] << std::endl;
    std::cout << "CoatColor: " << (int)rsi["CoatColor"] << std::endl;
    std::cout << "ShoeColor: " << (int)rsi["ShoeColor"] << std::endl;
    std::cout << "GlassesColor: " << (int)rsi["GlassesColor"] << std::endl;
    return 0;
}
