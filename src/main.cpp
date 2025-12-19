
#include <iostream>
#include <fcntl.h>
#include <Windows.h>
#include <io.h>
#include "xml.h"

using namespace std;
int main(int argc, wchar_t** argv) {
    _setmode(_fileno(stdout), _O_U16TEXT);

    std::wstring path =  L"./scenes/scene_00.xml";
    //std::wstring path =  L"./scenes/scene_01.xml";

    std::vector<xml_component> components;
    std::wstring result = read_xml(components, path);
    if (result != L"")
    {
        wcout << "Error reading XML: " << result << endl;
        return -1;
    }
    wcout << pprint_components(components);

    return 0;
}




