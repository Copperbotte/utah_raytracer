
#ifndef XML_COMPILER_HPP
#define XML_COMPILER_HPP

#ifndef SAFE_DELETE
#define SAFE_DELETE(ptr) \
    do { \
        if ((ptr) != nullptr) { \
            delete[] (ptr); \
            (ptr) = nullptr; \
        } \
    } while (0)
#endif

#include <string>
#include <charconv>
#include <stdexcept>
#include <algorithm>
#include "xml.hpp"

int search_sorted_index(int *map_rev, int map_len, int id);
void radix_sort(int *map, int map_len);

template<typename T>
struct kv_map
{
    T *items;
    int *map;
    int *imap_v;
    int *imap_k;
    int len;

    kv_map()
     : items(nullptr), map(nullptr), imap_v(nullptr), imap_k(nullptr), len(0)
    {}

    // Wrapped access: supports negative indices and equivalence class
    // arr[k] == arr[len * n + k] for any integers n,k (when len>0)
    T& operator[](int index) {
        if (items == nullptr || len <= 0) {
            throw std::out_of_range("kv_map index out of range");
        }
        int m = index % len;
        if (m < 0) m += len;
        return items[m];
    }

    // Const wrapped access
    const T& operator[](int index) const {
        if (items == nullptr || len <= 0) {
            throw std::out_of_range("kv_map index out of range");
        }
        int m = index % len;
        if (m < 0) m += len;
        return items[m];
    }

    void init()
    {
        items  = new T[len];
        map    = new int[len];
        imap_v = new int[len];
        imap_k = new int[len];
    }

    // I need to fill the reverse mappings:
    // spheres_map -> spheres_map_rev
    // cameras_map -> cameras_map_rev
    // Each forward map is guaranteed to be sorted (contiguous indices)
    // [0,1,2,3] -> [i0,i1,i2,i3]
    // The reverse map takes a sorted [i0,i1,i2,i3] and maps to [0,1,2,3].
    // Using a psuedo-merge sort?
    void init_reverse_index()
    {
        // std::wcout << L"init_reverse_index" << std::endl;
        // auto t0 = std::chrono::high_resolution_clock::now();
        memcpy(imap_v, map, sizeof(int)*len);
        // auto t1 = std::chrono::high_resolution_clock::now();

        //radix_sort(imap_v, len);
        std::sort(imap_v, imap_v + len);//#include <algorithm>
        
        // auto t2 = std::chrono::high_resolution_clock::now();
        for(int n=0; n<len; n++)
        {
            int v = map[n];
            int k = search_sorted_index(imap_v, len, v);
            imap_k[k] = n;
        }
        // auto t3 = std::chrono::high_resolution_clock::now();
        // typedef std::chrono::nanoseconds nanoseconds;
        // auto d10 = std::chrono::duration_cast<nanoseconds>(t1 - t0).count();
        // auto d21 = std::chrono::duration_cast<nanoseconds>(t2 - t1).count();
        // auto d32 = std::chrono::duration_cast<nanoseconds>(t3 - t2).count();
        // auto d30 = std::chrono::duration_cast<nanoseconds>(t3 - t0).count();
        // std::wcout << L"memcpy: " << d10 << L" ns\n";
        // std::wcout << L"radix_sort: " << d21 << L" ns\n";
        // std::wcout << L"search_sorted_index: " << d32 << L" ns\n";
        // std::wcout << L"init_reverse_index: " << d30 << L" ns\n";
        // std::wcout << std::endl;
    }

    ~kv_map()
    {
        // std::wcout << L"Memory leak check!!!" << std::endl;
        // std::wcout << L"items:  " << items << std::endl;
        // std::wcout << L"map:    " << map << std::endl;
        // std::wcout << L"imap_v: " << imap_v << std::endl;
        // std::wcout << L"imap_k: " << imap_k << std::endl;
        SAFE_DELETE(items);
        SAFE_DELETE(map);
        SAFE_DELETE(imap_v);
        SAFE_DELETE(imap_k);
        // std::wcout << L"items:  " << items << std::endl;
        // std::wcout << L"map:    " << map << std::endl;
        // std::wcout << L"imap_v: " << imap_v << std::endl;
        // std::wcout << L"imap_k: " << imap_k << std::endl;
        len = 0;
    }

};

struct sphere
{
    int entity, parent;
    std::wstring name;
    float radius;
    float pos[3];
};

struct camera
{
    int entity, parent;
    float pos[3];
    float target[3];
    float up[3];
    float fov_deg;
    int w,h;
};

struct xml_ir_float1
{
    int entity;
    std::wstring key;
    float x;
};

struct xml_ir_float3
{
    int entity;
    std::wstring key;
    float x;
    float y;
    float z;
};

struct xml_ir_string
{
    int entity;
    std::wstring key;
    std::wstring value;
};

enum Entity_Type{
    ENT_Sphere = 0,
    ENT_Camera,
};

struct renderables
{
    kv_map<int> entities;
    kv_map<sphere> spheres;
    kv_map<camera> cameras;
    kv_map<xml_ir_float1> float1s;
    kv_map<xml_ir_float3> float3s;
    kv_map<xml_ir_string> strings;

    void init()
    {
        //std::wcout << L"Renderables.init called" << std::endl;
        entities.init();
        spheres.init();
        cameras.init();
        float1s.init();
        float3s.init();
        strings.init();
    }
    
    void init_reverse_index()
    {
        entities.init_reverse_index();
        spheres.init_reverse_index();
        cameras.init_reverse_index();
        float1s.init_reverse_index();
        float3s.init_reverse_index();
        strings.init_reverse_index();
    }
};

inline std::string narrow_ascii(const std::wstring& wstr)
{
    std::string str;
    str.reserve(wstr.size());
    for (wchar_t wc : wstr)
        str.push_back(static_cast<char>(wc));
    return str;
}

template<typename T>
T decode(const std::wstring& value, bool &success)
{
    T out;
    std::string s = narrow_ascii(value);
    auto [ptr, ec] = std::from_chars(s.c_str(), s.c_str() + s.size(), out);
    success = (ec == std::errc() && ptr == s.c_str() + s.size());
    return out;
}

template<typename T>
T decode(const std::wstring& value)
{
    T out;
    bool success;
    out = decode<T>(value, success);
    if (!success)
        throw std::runtime_error("Failed to decode value: " + narrow_ascii(value));
    return out;
}

void init_renderables(renderables &Renderables, const std::vector<xml_component> &components);
void dump_renderables(const renderables& r);
void dump_renderables(const renderables& r, int max_items);

#endif // XML_COMPILER_HPP
