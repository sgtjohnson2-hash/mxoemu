#ifndef MXOEMU_ITEMSERIALIZER_H
#define MXOEMU_ITEMSERIALIZER_H

#include "Item.h"
#include <string>
#include <memory>

class ItemSerializer
{
public:
    static std::string Serialize(std::shared_ptr<Item> item);
    static void Deserialize(std::shared_ptr<Item> item, const std::string& metadata);
};

#endif // MXOEMU_ITEMSERIALIZER_H
