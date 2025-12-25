
#include "xml.hpp"
#include <sstream>
#include <fstream>

std::wstring strip(const std::wstring& str)
{
    size_t first = str.find_first_not_of(L" \t\n\r");
    if (first == std::wstring::npos)
        return L"";
    size_t last = str.find_last_not_of(L" \t\n\r");
    return str.substr(first, (last - first + 1));
}

struct bound
{
    int start, stop;
    bound() : start(-1), stop(-1) {}
    bound(int s, int e) : start(s), stop(e) {}
    bound(const bound& b) : start(b.start), stop(b.stop) {}

    bound& operator=(const bound& b)
    {
        if (this != &b)
        {
            start = b.start;
            stop = b.stop;
        }
        return *this;
    }
};

struct xml_tag
{
    std::wstring name;
    std::wstring attributes;
    bool self_closing;
    bool is_closing;
    int cid;
};

enum xml_quote_type
{
    XML_QUOTE_SINGLE = 0,
    XML_QUOTE_DOUBLE = 1
};
const wchar_t xml_quote_type_LUT[2] = {L'\'', L'"'};

struct xml_tag_item
{
    xml_tag tag;
    int component_id;
    xml_tag_item* next, *prev;
    xml_tag_item() : next(nullptr), prev(nullptr) {}
    xml_tag_item(const xml_tag& t) : tag(t), next(nullptr), prev(nullptr) {}
    xml_tag_item(xml_tag_item *prev, xml_tag t) : tag(t), next(nullptr), prev(prev)
    {
        if (prev)
            prev->next = this;
    }
    xml_tag_item& operator=(const xml_tag_item& item)
    {
        if (this != &item)
        {
            tag = item.tag;
            component_id = item.component_id;
            next = item.next;
            prev = item.prev;
        }
        return *this;
    }

};


bound find_next_tag(const std::wstring& str, int start_pos)
{
    bound result = bound(start_pos, str.length());
    size_t open_pos = str.find(L"<", start_pos);
    if (open_pos == std::wstring::npos)
        return result;
    size_t close_pos = str.find(L">", open_pos);
    if (close_pos == std::wstring::npos)
        return result;
    result.start = static_cast<int>(open_pos);
    result.stop = static_cast<int>(close_pos);
    return result;
}



// void for now
xml_tag parse_next_tag(const std::wstring& tag_str, int *start_pos)
{
    bound tag_bound = find_next_tag(tag_str, *start_pos);
    if (tag_bound.start == -1 || tag_bound.stop == -1)
        return xml_tag();

    std::wstring tag_full = tag_str.substr(tag_bound.start, tag_bound.stop - tag_bound.start + 1);
    std::wstring tag_contents = strip(tag_full).substr(1, tag_full.length() - 2); // Remove < and >

    std::wstring tag_name;
    xml_tag tag;
    tag.name = tag_contents;
    tag.self_closing = false;
    tag.is_closing = false;

    // Check if tag is a closing tag
    if(tag.name[0] == L'/')
    {
        tag.is_closing = true;
        tag.name = strip(tag.name.substr(1));
    }

    // Check if tag is self-closing
    if(tag.name[tag.name.length() - 1] == L'/')
    {
        // Self-closing tag
        tag.name = tag.name.substr(0, tag.name.length() - 1);
        tag.self_closing = true;
    }

    tag_contents = strip(tag.name);

    // Find first space if attributes exist
    int space_pos = tag.name.find(L" ");
    if(space_pos == std::wstring::npos)
        space_pos = tag.name.length();

    tag.name = tag.name.substr(0, space_pos);
    tag.attributes = strip(tag_contents.substr(space_pos));

    // Return tag and update start_pos
    *start_pos = tag_bound.stop + 1;
    return tag;
}

xml_component parse_next_attribute(const std::wstring& attr_str, int *start_pos)
{
    // Attributes are always in the form `key="val"`, seperated by a space.
    size_t equal_pos = attr_str.find(L"=", *start_pos);

    xml_component attr;
    attr.key = strip(attr_str.substr(*start_pos, equal_pos - *start_pos));

    // Find the quote pair
    xml_quote_type quote_type;
    if(attr_str[equal_pos + 1] == L'\'') quote_type = XML_QUOTE_SINGLE;
    else quote_type = XML_QUOTE_DOUBLE;

    wchar_t delim = xml_quote_type_LUT[quote_type];
    size_t quote_start = attr_str.find(delim, equal_pos + 1) + 1;
    size_t quote_end = attr_str.find(delim, quote_start + 1) - 1;
    attr.value = strip(attr_str.substr(quote_start, quote_end - quote_start + 1));

    // Update start_pos
    *start_pos = static_cast<int>(quote_end + 2);
    return attr;
}

std::wstring parse_all_attributes(const std::wstring& attr_str, std::vector<xml_component> *components, int parent_id)//, std::vector<xml_component>* out_attributes)
{
    if(attr_str.find(L"=") == std::wstring::npos)
        return L"N/A";

    std::wstring result = L"";
    int pos = 0;
    while (true)
    {
        if(attr_str.find(L"=", pos) == std::wstring::npos) break;
        if(pos != 0) result += L", ";

        xml_component attr = parse_next_attribute(attr_str, &pos);
        attr.id = components->size();
        attr.parent_id = parent_id;
        components->push_back(attr);

        result += attr.key + L":" + attr.value;
        if(attr_str.find(L"=", pos) == std::wstring::npos) // break if no more '=' found
            break;
    }
    return result;
}

std::vector<xml_component> parse_all_tags(const std::wstring& str, const std::wstring& path)
{
    std::vector<xml_component> components;

    int pos = 0;
    xml_tag tag = parse_next_tag(str, &pos);
    tag.cid = components.size();
    xml_component tag_comp(tag.cid, -1, L"tag", tag.name);
    components.push_back(tag_comp);
    int parent_id = 0;

    // Manually add a name component for the xml path
    tag_comp = xml_component(components.size(), parent_id, L"name", path);
    components.push_back(tag_comp);

    while (true)
    {
        tag = parse_next_tag(str, &pos);
        if(!tag.is_closing)
        {
            // Manually add a component for the tag we found
            tag_comp = xml_component(components.size(), parent_id, L"tag", tag.name);
            components.push_back(tag_comp);

            parse_all_attributes(tag.attributes, &components, tag_comp.id);
            if(!tag.self_closing)
                parent_id = tag_comp.id;
        }
        else
            parent_id = components[parent_id].parent_id;

        if(tag.is_closing && tag.name == L"xml")
            break;
    }

    return components;
}

std::wstring pprint_components(const std::vector<xml_component>& components)
{
    std::wstring result = L"";
    result += L"Parsed " + std::to_wstring(components.size()) + L" components.\n";

    
    // Print all components and the parent's name they're attached to
    // parent <- key: value
    for (const auto& comp : components)
    {
        std::wstring parent_name = L"N/A";
        if (comp.parent_id != -1)
            parent_name = components[comp.parent_id].value;
        result += parent_name + L" <- " + comp.key + L": " + comp.value + L"\n";
    }
    result += L"\n";

    // Compute "adjacency" levels for pretty printing by accumulating the number of children
    std::vector<int> leaves(components.size(), 0);
    for (const auto& comp : components)
        if (comp.parent_id != -1)
            leaves[comp.parent_id]++;
    std::vector<int> leaves_clone = leaves;

    // Print all components and the parent's name they're attached to
    // parent <- key: value
    //for (const xml_component& comp : components)
    for (int n=0; n<components.size(); ++n)
    {
        const xml_component& comp = components[n];

        std::wstring parent_name = L"N/A";
        if (comp.parent_id != -1)
            parent_name = components[comp.parent_id].value;

        // Get the number of tabs required to reach the root (-1)
        int level = 0;
        int parent = comp.parent_id;
        std::wstring indent = L"";
        while (parent != -1)
        {
            ++level;
            if(0 < leaves[parent])
                indent = L"│ " + indent;
            else
                indent = L"  " + indent;
            parent = components[parent].parent_id;
        }
        //replace the last two characters with L"└──"
        if (2 <= indent.length())
        {
            indent = indent.substr(0, indent.length() - 2);
            leaves[comp.parent_id]--;
            if(0 < leaves[comp.parent_id]) indent += L"├→";//L"├─";
            else                           indent += L"└→";//L"└─";
            //indent += std::to_wstring(leaves[comp.parent_id]) + L"─";
        }

        // If the object is a tag, only show value
        if (components[comp.id].key == L"tag" || comp.parent_id == -1)
            result += indent + comp.value;// + L"\n";
        else
            result += indent + comp.key + L": " + comp.value;// + L"\n";
        
        // If there are no children here, print that, otherwise return.
        //if(leaves_clone[n] == 0) result += L" <-- Data!";
        result += L"\n";
    }

    return result;
}

std::wstring read_xml(std::vector<xml_component>& components, const std::wstring& path)
{
    std::wifstream file(path.c_str());
    if (!file.is_open()) {
        return L"Error opening file";
    }

    std::wstring content((std::istreambuf_iterator<wchar_t>(file)),
                          std::istreambuf_iterator<wchar_t>());
    file.close();

    // Purge XML comments: remove all <!-- ... --> blocks (may be multiline)
    size_t pos = 0;
    while (true)
    {
        size_t start = content.find(L"<!--", pos);
        if (start == std::wstring::npos) break;
        size_t end = content.find(L"-->", start + 4);
        if (end == std::wstring::npos)
        {
            // unterminated comment: remove until end of string
            content.erase(start);
            break;
        }
        // erase comment including the closing "-->"
        content.erase(start, end - start + 3);
        // continue scanning from the same start position (new content there)
        pos = start;
    }

    components = parse_all_tags(content, path);

    return L"";
}

std::wstring xml_test()
{
    std::wstring path =  L"./scenes/scene_00.xml";
    //std::wstring path =  L"./scenes/scene_01.xml";

    std::vector<xml_component> components;
    std::wstring result = read_xml(components, path);
    if (result != L"")
        return result;
    return pprint_components(components);
}
