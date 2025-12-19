
#ifndef XML_HPP
#define XML_HPP

#include <string>
#include <vector>

struct xml_component
{
    int id, parent_id;
    std::wstring key, value;

    xml_component() : id(-1), parent_id(-1), key(L""), value(L"") {}
    xml_component(int i, int p, const std::wstring& k, const std::wstring& v) :
        id(i), parent_id(p), key(k), value(v) {}
    xml_component(const xml_component& c) :
        id(c.id), parent_id(c.parent_id), key(c.key), value(c.value) {}
    xml_component& operator=(const xml_component& c)
    {
        if (this != &c)
        {
            id = c.id;
            parent_id = c.parent_id;
            key = c.key;
            value = c.value;
        }
        return *this;
    }
};

std::wstring pprint_components(const std::vector<xml_component>& components);
std::wstring read_xml(std::vector<xml_component>& components, const std::wstring& path);
std::wstring xml_test();

#endif // XML_HPP
