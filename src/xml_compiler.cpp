
#include "xml_compiler.hpp"
#include <iostream>
#include <sstream>
#include <cstring>
#include <type_traits>

// map forward maps continuous indices [0,1,2,3] -> [i0,i1,i2,i3]
// search sorted could work there, but there's no need
int search_sorted_index(int *map_rev, int map_len, int id)
{
    if (!map_rev || map_len <= 0) return -1;
    int lo = 0, hi = map_len - 1;
    while (lo <= hi) {
        int mid = lo + ((hi - lo) >> 1);
        int v = map_rev[mid];
        if (v == id) return mid;
        if (v < id) lo = mid + 1;
        else hi = mid - 1;
    }
    return -1;
}

// Binary LSD radix sort for non-negative ints.
// Uses an auxiliary buffer so the number of passes is finite (number of bits in max element).
void radix_sort(int *map, int map_len)
{
    if (!map || map_len <= 1) return;

    // find maximum value to know how many bits we need
    unsigned int maxv = 0;
    for (int i = 0; i < map_len; ++i) {
        if (map[i] < 0) {
            // negative values are not expected for index maps; treat them as large unsigned
            maxv = ~0u;
            break;
        }
        if ((unsigned int)map[i] > maxv) maxv = (unsigned int)map[i];
    }
    if (maxv == 0) return; // all zeros

    // compute number of bits needed
    int passes = 0;
    while (maxv) { maxv >>= 1; ++passes; }

    int *tmp = new int[map_len];
    for (int bit = 0; bit < passes; ++bit) {
        // count zeros for this bit (stable partition)
        int zero_count = 0;
        for (int i = 0; i < map_len; ++i) {
            if (((unsigned int)map[i] >> bit & 1u) == 0u) ++zero_count;
        }

        int zpos = 0;
        int opos = zero_count;
        for (int i = 0; i < map_len; ++i) {
            if (((unsigned int)map[i] >> bit & 1u) == 0u)
                tmp[zpos++] = map[i];
            else
                tmp[opos++] = map[i];
        }

        // copy back
        for (int i = 0; i < map_len; ++i) map[i] = tmp[i];
    }
    delete[] tmp;
}

bool xml_kv_cmp(const xml_component& comp, const std::wstring &key, const std::wstring &val)
{
    return comp.key == key && comp.value == val;
}

int find_xml_entity_parent(
    const std::vector<xml_component> &components,
    const kv_map<int> &entities,
    int xml_id)
{
    // Ascend until we find an object or a camera. Ignore xml and negatives.
    while(0 < xml_id)
    {
        int eid = search_sorted_index(entities.imap_v, entities.len, xml_id);
        if(-1 != eid)
            return eid;//xml_id;
        xml_id = components[xml_id].parent_id;
    }
    return -1;
}

void init_renderables(renderables &Renderables, const std::vector<xml_component> &components)
{
    // Gather child adjacency list (may be useful)
    std::vector<int> xml_leaves(components.size(), 0);
    for (const xml_component& comp : components)
        if (comp.parent_id != -1)
            xml_leaves[comp.parent_id]++;

    const std::vector<std::wstring> float1_keys = {
        //L"scale", L"fov", L"width", L"height"
        L"fov", L"width", L"height",
        
        // NOTE: THE FOLLOWING ARE IN BOTH TO ENCODE FLOAT4S
        L"specular", L"rotation",
    };
    const std::vector<std::wstring> float3_keys = {
        L"scale", L"translate", L"position", L"target", L"up",
        L"diffuse", L"glossiness", //L"specular",
        L"intensity", L"direction",

        // NOTE: THE FOLLOWING ARE IN BOTH TO ENCODE FLOAT4S
        L"specular", L"rotation",
    };
    const std::vector<std::wstring> string_keys = {
        L"type", L"name", L"material",
    };

    // Gather renderable instance counts
    //for (const xml_component& comp : components)
    for(int n=0; n<components.size(); n++)
    {
        const xml_component& comp = components[n];
        //
        // Objects
        //if(0 < comp.parent_id){
        //    const xml_component prnt = components[comp.parent_id];
        //    if(xml_kv_cmp(comp, L"type", L"object") && xml_kv_cmp(prnt, L"tag", L"object"))
        //        Renderables.objects.len++;
        //}
        if(xml_kv_cmp(comp, L"tag", L"object"))
            Renderables.objects.len++;
        if(xml_kv_cmp(comp, L"tag", L"material"))
            Renderables.materials.len++;
        if(xml_kv_cmp(comp, L"tag", L"light"))
            Renderables.lights.len++;

        if(xml_kv_cmp(comp, L"tag", L"camera"))
            Renderables.cameras.len++;
        
        // Components
        for(const std::wstring& key : float1_keys)
            if(xml_kv_cmp(comp, L"tag", key))
                Renderables.float1s.len++;
        for(const std::wstring& key : float3_keys)
            if(xml_kv_cmp(comp, L"tag", key))
                Renderables.float3s.len++;
        for(const std::wstring& key : string_keys) // Note: this should grab type from object. This is intentional!
            if(comp.key == key) // Values are irrelevant?
                Renderables.strings.len++;
    }

    Renderables.entities.len = Renderables.objects.len + Renderables.materials.len
         + Renderables.lights.len + Renderables.cameras.len;

    std::wcout << L"Entities found: " << Renderables.entities.len << std::endl;
    std::wcout << L"Objects found: " << Renderables.objects.len << std::endl;
    std::wcout << L"Materials found: " << Renderables.materials.len << std::endl;
    std::wcout << L"Lights found: " << Renderables.lights.len << std::endl;
    std::wcout << L"Cameras found: " << Renderables.cameras.len << std::endl;
    std::wcout << L"Float1 components found: " << Renderables.float1s.len << std::endl;
    std::wcout << L"Float3 components found: " << Renderables.float3s.len << std::endl;
    std::wcout << L"String components found: " << Renderables.strings.len << std::endl;

    Renderables.init();

    // Init object scales to be all ones >:(
    for(int n=0; n<Renderables.objects.len; n++)
        for(int k=0; k<3; k++)
            Renderables.objects[n].scale[k] = 1.0f;

    int objects_visited = 0;
    int materials_visited = 0;
    int lights_visited = 0;
    int cameras_visited = 0;
    int float1s_visited = 0;
    int float3s_visited = 0;
    int strings_visited = 0;

    // Repeat but find object mappings
    //for(int n=0; n<(int)components.size(); ++n)
    for (const xml_component& comp : components)
    {
        //const xml_component& comp = components[n];
        int n = comp.id;
        int entities_visited = objects_visited + cameras_visited + 
            lights_visited + materials_visited;

        if(comp.parent_id == -1) continue;

        const xml_component& prnt = components[comp.parent_id];
        //if (xml_kv_cmp(comp, L"type", L"object") && xml_kv_cmp(prnt, L"tag", L"object"))
        if(xml_kv_cmp(comp, L"tag", L"object"))
        {
            Renderables.objects.map[objects_visited] = n;//prnt.id; // Dont use n for this one
            Renderables.objects[objects_visited].entity = entities_visited;
            Renderables.entities.map[entities_visited] = n;//prnt.id;
            Renderables.entities[entities_visited] = ENT_Object;

            objects_visited++;
            continue;
        }

        if(xml_kv_cmp(comp, L"tag", L"material"))
        {
            Renderables.materials.map[materials_visited] = n;
            Renderables.materials[materials_visited].entity = entities_visited;
            Renderables.entities.map[entities_visited] = n;
            Renderables.entities[entities_visited] = ENT_Material;

            materials_visited++;
            continue;
        }

        if(xml_kv_cmp(comp, L"tag", L"light"))
        {
            Renderables.lights.map[lights_visited] = n;
            Renderables.lights[lights_visited].entity = entities_visited;
            Renderables.entities.map[entities_visited] = n;
            Renderables.entities[entities_visited] = ENT_Light;

            lights_visited++;
            continue;
        }

        if(xml_kv_cmp(comp, L"tag", L"camera"))
        {
            Renderables.cameras.map[cameras_visited] = n;
            Renderables.cameras[cameras_visited].entity = entities_visited;
            Renderables.entities.map[entities_visited] = n;
            Renderables.entities[entities_visited] = ENT_Camera;

            cameras_visited++;
            continue;
        }
    }

    Renderables.entities.init_reverse_index();
    Renderables.objects.init_reverse_index();
    Renderables.materials.init_reverse_index();
    Renderables.lights.init_reverse_index();
    Renderables.cameras.init_reverse_index();

    // Repeat but find component mappings
    for(int n=0; n<(int)components.size(); ++n)
    {
        const xml_component& comp = components[n];

        for(const std::wstring& key : float1_keys)
        {
            if(xml_kv_cmp(comp, L"tag", key))
            {
                Renderables.float1s.map[float1s_visited] = n;
                Renderables.float1s[float1s_visited].key = key;
                Renderables.float1s[float1s_visited].entity = 
                    find_xml_entity_parent(components, Renderables.entities, n);
                float1s_visited++;
                break;
            }
        }
        for(const std::wstring& key : float3_keys)
        {
            if(xml_kv_cmp(comp, L"tag", key))
            {
                Renderables.float3s.map[float3s_visited] = n;
                Renderables.float3s[float3s_visited].key = key;
                Renderables.float3s[float3s_visited].entity = 
                    find_xml_entity_parent(components, Renderables.entities, n);
                float3s_visited++;
                break;
            }
        }
        for(const std::wstring& key : string_keys)
        {
            if(comp.key == key)
            {
                Renderables.strings.map[strings_visited] = n;
                Renderables.strings[strings_visited].key = key;
                Renderables.strings[strings_visited].value = comp.value; // I might as well do this here
                Renderables.strings[strings_visited].entity = 
                    find_xml_entity_parent(components, Renderables.entities, n);
                strings_visited++;
                break;
            }
        }
    }

    //Renderables.init_reverse_index();
    Renderables.float1s.init_reverse_index();
    Renderables.float3s.init_reverse_index();
    Renderables.strings.init_reverse_index();

    // Fill component data
    for(int nn=0; nn<components.size(); ++nn)
    {
        int n = (components.size()-1)-nn; // Reverse order
        const xml_component& comp = components[n];

        if(xml_leaves[n] != 0) continue; // Data! Ascend and fill a slot.

        // Find where this data lives.
        int pid = comp.parent_id;
        int eid = -1;
        eid = search_sorted_index(Renderables.float1s.imap_v, Renderables.float1s.len, pid);
        if(-1 != eid) // This is a float1!
        {
            xml_ir_float1 &f1 = Renderables.float1s[eid];
            float val = decode<float>(comp.value);
            if(f1.key != L"specular" && f1.key != L"rotation")
            {
                Renderables.float1s[eid].x = val;
                continue;
            }
            else if(comp.key == L"value" || comp.key == L"angle")
            {
                Renderables.float1s[eid].x = val;
                continue;
            }
            //std::wcout << L"Specular Found! " << f1.key << L" " << f1.x << L" " << comp.key << L" " << comp.value << std::endl;
            //if(f1.)
            
        }

        eid = search_sorted_index(Renderables.float3s.imap_v, Renderables.float3s.len, pid);
        if(-1 != eid) // This is a float3!
        {
            xml_ir_float3 &f3 = Renderables.float3s[eid];
            float val = decode<float>(comp.value);
            // Determine the float3 slot via a common mapping
            if(comp.key == L"x" || comp.key == L"r") {f3.x = val; continue;}
            if(comp.key == L"y" || comp.key == L"g") {f3.y = val; continue;}
            if(comp.key == L"z" || comp.key == L"b") {f3.z = val; continue;}

            // Oddball mapping: scale. Scale can be a float1, but we'll encode it as
            // a float3, as it can *sometimes* have 3 children. When its a float3, its
            // handled by the code above. But, if its a float1, it needs to fill all
            // three slots. This should only fire if any of the previous ones fail.
            if(f3.key == L"scale" || f3.key == L"glossiness" || f3.key == L"intensity")
            {
                f3.x = val;
                f3.y = val;
                f3.z = val;
            }
            continue;
        }

        // Did this earlier. Perhaps that's a bad move.
        // std::wcout << comp.key << L" " << comp.value << std::endl;
        // eid = search_sorted_index(Renderables.strings.imap_v, Renderables.strings.len, pid);
        // if(-1 != eid) // This is a string!
        // {
        //     Renderables.strings[eid].value = comp.value;
        //     continue;
        // }
    }

    // Fill entity data
    // for(int nn=0; nn<components.size(); ++nn)
    // {
    //     int n = (components.size()-1)-nn; // Reverse order
    //     const xml_component& comp = components[n];
    //     //Renderables.strings[strings_visited].entity = 
    //     //    find_xml_entity_parent(components, Renderables.entities, n);
    //     
    // }

    // Fill entity data with components directly!!!
    for(int n=0; n<Renderables.strings.len; ++n)
    {
        const xml_ir_string &s = Renderables.strings.items[n];
        int ent_type = Renderables.entities[s.entity];
        if(ent_type == ENT_Object)
        {
            if(s.key == L"name") Renderables.objects[s.entity].name = s.value;
            if(s.key == L"type") Renderables.objects[s.entity].type = s.value;
            if(s.key == L"material") Renderables.objects[s.entity].mat = s.value;
        }
        if(ent_type == ENT_Material)
        {
            if(s.key == L"name") Renderables.materials[s.entity].name = s.value;
            if(s.key == L"type") Renderables.materials[s.entity].type = s.value;
        }
        if(ent_type == ENT_Light)
        {
            if(s.key == L"name") Renderables.lights[s.entity].name = s.value;
            if(s.key == L"type") Renderables.lights[s.entity].type = s.value;
        }
        if(ent_type == ENT_Camera)
            ; // Camera has no name
    }

    
    for(int n=0; n<Renderables.float1s.len; ++n)
    {
        const xml_ir_float1 &s = Renderables.float1s.items[n];
        int ent_type = Renderables.entities[s.entity];
        if(ent_type == ENT_Object)
        {
            //if(s.key == L"scale") Renderables.objects[s.entity].radius = s.x;
            if(s.key == L"rotation") Renderables.objects[s.entity].rotation[3] = s.x * (3.141592653589793f/180.0f); // Easiest spot to do the conversion I suppose
        }
        if(ent_type == ENT_Camera)
        {
            if(s.key == L"fov") Renderables.cameras[s.entity].fov_deg = s.x;
            if(s.key == L"width") Renderables.cameras[s.entity].w = s.x;
            if(s.key == L"height") Renderables.cameras[s.entity].h = s.x;
        }
        if(ent_type == ENT_Material)
        {
            if(s.key == L"specular") Renderables.materials[s.entity].glossiness_value = s.x;
        }
    }

    
    for(int n=0; n<Renderables.float3s.len; ++n)
    {
        const xml_ir_float3 &s = Renderables.float3s.items[n];
        int ent_type = Renderables.entities[s.entity];
        if(ent_type == ENT_Object)
        {
            if(s.key == L"translate")
            {
                Renderables.objects[s.entity].pos[0] = s.x;
                Renderables.objects[s.entity].pos[1] = s.y;
                Renderables.objects[s.entity].pos[2] = s.z;
            }
            if(s.key == L"scale")
            {
                Renderables.objects[s.entity].scale[0] = s.x;
                Renderables.objects[s.entity].scale[1] = s.y;
                Renderables.objects[s.entity].scale[2] = s.z;
            }
            if(s.key == L"rotation")
            {
                Renderables.objects[s.entity].rotation[0] = s.x;
                Renderables.objects[s.entity].rotation[1] = s.y;
                Renderables.objects[s.entity].rotation[2] = s.z;
            }
        }
        if(ent_type == ENT_Material)
        {
            if(s.key == L"diffuse")
            {
                Renderables.materials[s.entity].albedo[0] = s.x;
                Renderables.materials[s.entity].albedo[1] = s.y;
                Renderables.materials[s.entity].albedo[2] = s.z;
            }
            if(s.key == L"specular")
            {
                Renderables.materials[s.entity].spec_color[0] = s.x;
                Renderables.materials[s.entity].spec_color[1] = s.y;
                Renderables.materials[s.entity].spec_color[2] = s.z;
            }
            if(s.key == L"glossiness")
            {
                Renderables.materials[s.entity].glossiness[0] = s.x;
                Renderables.materials[s.entity].glossiness[1] = s.y;
                Renderables.materials[s.entity].glossiness[2] = s.z;
            }
        }
        if(ent_type == ENT_Light)
        {
            if(s.key == L"intensity")
            {
                Renderables.lights[s.entity].intensity[0] = s.x;
                Renderables.lights[s.entity].intensity[1] = s.y;
                Renderables.lights[s.entity].intensity[2] = s.z;
            }
            if(s.key == L"direction")
            {
                Renderables.lights[s.entity].direction[0] = s.x;
                Renderables.lights[s.entity].direction[1] = s.y;
                Renderables.lights[s.entity].direction[2] = s.z;
            }
            if(s.key == L"position")
            {
                Renderables.lights[s.entity].position[0] = s.x;
                Renderables.lights[s.entity].position[1] = s.y;
                Renderables.lights[s.entity].position[2] = s.z;
            }
        }
        if(ent_type == ENT_Camera)
        {
            if(s.key == L"position")
            {
                Renderables.cameras[s.entity].pos[0] = s.x;
                Renderables.cameras[s.entity].pos[1] = s.y;
                Renderables.cameras[s.entity].pos[2] = s.z;
            } 
            if(s.key == L"target")
            {
                Renderables.cameras[s.entity].target[0] = s.x;
                Renderables.cameras[s.entity].target[1] = s.y;
                Renderables.cameras[s.entity].target[2] = s.z;
            }
            if(s.key == L"up")
            {
                Renderables.cameras[s.entity].up[0] = s.x;
                Renderables.cameras[s.entity].up[1] = s.y;
                Renderables.cameras[s.entity].up[2] = s.z;
            } 
        }
    }

    // Assign parents based on xml adjacency
    for (int n=0; n<Renderables.objects.len; ++n)
    {
        int xml_root = Renderables.objects.map[n];
        int xml_parent = components[xml_root].parent_id;
        int parent = find_xml_entity_parent(components, Renderables.entities, xml_parent);
        Renderables.objects[n].parent = parent; // -1 is none
    }
    for (int n=0; n<Renderables.materials.len; ++n)
    {
        int xml_root = Renderables.materials.map[n];
        int xml_parent = components[xml_root].parent_id;
        int parent = find_xml_entity_parent(components, Renderables.entities, xml_parent);
        Renderables.materials[n].parent = parent; // -1 is none
    }
    for (int n=0; n<Renderables.lights.len; ++n)
    {
        int xml_root = Renderables.lights.map[n];
        int xml_parent = components[xml_root].parent_id;
        int parent = find_xml_entity_parent(components, Renderables.entities, xml_parent);
        Renderables.lights[n].parent = parent; // -1 is none
    }
    for (int n=0; n<Renderables.cameras.len; ++n)
    {
        int xml_root = Renderables.cameras.map[n];
        int xml_parent = components[xml_root].parent_id;
        int parent = find_xml_entity_parent(components, Renderables.entities, xml_parent);
        Renderables.cameras[n].parent = parent; // -1 is none
    }

    // Repurpose renderables for an entity->object lookup
    for (int n=0; n<Renderables.objects.len;   ++n) Renderables.objects.map[  n] =   Renderables.objects.items[n].entity;
    for (int n=0; n<Renderables.materials.len; ++n) Renderables.materials.map[n] = Renderables.materials.items[n].entity;
    for (int n=0; n<Renderables.lights.len;    ++n) Renderables.lights.map[   n] =    Renderables.lights.items[n].entity;
    for (int n=0; n<Renderables.cameras.len;   ++n) Renderables.cameras.map[  n] =   Renderables.cameras.items[n].entity;
    Renderables.objects.init_reverse_index();
    Renderables.materials.init_reverse_index();
    Renderables.lights.init_reverse_index();
    Renderables.cameras.init_reverse_index();

    // For each entity, find its corresponding material and set object.mid. mid is the *material* index.
    // This is better done with a hashmap but I dont know how to do that in c++. It'll probably be fine.
    for (int s=0; s<Renderables.objects.len; ++s)
        for (int m=0; m<Renderables.materials.len; ++m)
            if(Renderables.materials.items[m].name == Renderables.objects.items[s].mat)
                Renderables.objects.items[s].mid = m;
}





// ChatGPT's testing functions

#define PRINTF3(NAME) << L" " << #NAME << L"=(" << s.NAME[0] << L"," << s.NAME[1] << L"," << s.NAME[2] << L")"

inline void print_entity(const int& ent, int idx)
{
    std::wcout
        << L"  [" << idx << L"]"
        << L" entity=" << ent
        << L"\n";
}

inline void print_object(const object& s, int idx)
{
    std::wcout
        << L"  [" << idx << L"] name=\"" << s.name << L"\""
        << L" entity=" << s.entity << L" parent=" << s.parent
        << L" type=" << s.type
        << L" material=" << s.mat
        PRINTF3(scale)//<< L" scale=(" << s.scale[0] << L"," << s.scale[1] << L"," << s.scale[2] << L")"
        PRINTF3(pos)//<< L" pos=(" << s.pos[0] << L"," << s.pos[1] << L"," << s.pos[2] << L")\n";
        << L"\n";
}


inline void print_material(const material& s, int idx)
{
    std::wcout
        << L"  [" << idx << L"] name=\"" << s.name << L"\""
        << L" entity=" << s.entity << L" parent=" << s.parent
        << L" type=" << s.type
        PRINTF3(albedo)//<< L" albedo=(" << s.albedo[0] << L"," << s.albedo[1] << L"," << s.albedo[2] << L")"
        PRINTF3(spec_color)//<< L" spec_color=(" << s.spec_color[0] << L"," << s.spec_color[1] << L"," << s.spec_color[2] << L")"
        PRINTF3(glossiness)//<< L" glossiness=(" << s.glossiness[0] << L"," << s.glossiness[1] << L"," << s.glossiness[2] << L")"
        << L" glossiness_value=" << s.glossiness_value
        << L"\n";
}

inline void print_light(const light& s, int idx)
{
    std::wcout
        << L"  [" << idx << L"] name=\"" << s.name << L"\""
        << L" entity=" << s.entity << L" parent=" << s.parent
        << L" type=" << s.type
        PRINTF3(intensity)//<< L" albedo=(" << s.albedo[0] << L"," << s.albedo[1] << L"," << s.albedo[2] << L")"
        PRINTF3(direction)//<< L" spec_color=(" << s.spec_color[0] << L"," << s.spec_color[1] << L"," << s.spec_color[2] << L")"
        PRINTF3(position)//<< L" glossiness=(" << s.glossiness[0] << L"," << s.glossiness[1] << L"," << s.glossiness[2] << L")"
        << L"\n";
}

inline void print_camera(const camera& c, int idx)
{
    std::wcout
        << L"  [" << idx << L"]"
        << L" entity=" << c.entity << L" parent=" << c.parent
        << L" pos=(" << c.pos[0] << L"," << c.pos[1] << L"," << c.pos[2] << L")"
        << L" target=(" << c.target[0] << L"," << c.target[1] << L"," << c.target[2] << L")"
        << L" up=(" << c.up[0] << L"," << c.up[1] << L"," << c.up[2] << L")"
        << L" fov=" << c.fov_deg
        << L" res=" << c.w << L"x" << c.h
        << L"\n";
}

inline void print_xml_ir_float1(const xml_ir_float1& v, int idx)
{
    std::wcout
        << L"  [" << idx << L"]"
        << L" entity=" << v.entity
        << L" key=\"" << v.key << L"\""
        << L" x=" << v.x
        << L"\n";
}

inline void print_xml_ir_float3(const xml_ir_float3& v, int idx)
{
    std::wcout
        << L"  [" << idx << L"]"
        << L" entity=" << v.entity
        << L" key=\"" << v.key << L"\""
        << L" x=(" << v.x << L"," << v.y << L"," << v.z << L")"
        << L"\n";
}

inline void print_xml_ir_string(const xml_ir_string& v, int idx)
{
    std::wcout
        << L"  [" << idx << L"]"
        << L" entity=" << v.entity
        << L" key=\"" << v.key << L"\""
        << L" value=\"" << v.value << L"\""
        << L"\n";
}


inline bool validate_kv_map_common(
    const wchar_t* label,
    const int* map,
    const int* imap_v,
    const int* imap_k,
    int len,
    bool verbose = true)
{
    bool ok = true;

    auto fail = [&](const std::wstring& msg) {
        ok = false;
        if (verbose) std::wcout << L"[FAIL] " << label << L": " << msg << L"\n";
    };

    if (len < 0) fail(L"len < 0");
    if (len == 0) return ok; // nothing else to validate

    if (!map)    fail(L"map is null (len>0)");
    if (!imap_v) fail(L"imap_v is null (len>0)");
    if (!imap_k) fail(L"imap_k is null (len>0)");
    if (!ok) return false;

    // range + uniqueness check for map
    std::vector<int> seen(len, 0);
    for (int i = 0; i < len; i++)
    {
        int v = map[i];
        if (v < 0 || v >= len)
        {
            std::wstringstream ss;
            ss << L"map[" << i << L"]=" << v << L" out of range [0," << (len-1) << L"]";
            fail(ss.str());
            continue;
        }
        if (seen[v]++)
        {
            std::wstringstream ss;
            ss << L"map contains duplicate value " << v << L" (at index " << i << L")";
            fail(ss.str());
        }
    }

    // imap_v should be sorted copy of map (nondecreasing and same multiset)
    for (int i = 1; i < len; i++)
    {
        if (imap_v[i-1] > imap_v[i])
        {
            std::wstringstream ss;
            ss << L"imap_v is not sorted at i=" << i
               << L" (" << imap_v[i-1] << L" > " << imap_v[i] << L")";
            fail(ss.str());
            break;
        }
    }

    // imap_k should invert imap_v: for each sorted position k, original index n satisfies map[n]=imap_v[k]
    for (int k = 0; k < len; k++)
    {
        int n = imap_k[k];
        if (n < 0 || n >= len)
        {
            std::wstringstream ss;
            ss << L"imap_k[" << k << L"]=" << n << L" out of range";
            fail(ss.str());
            continue;
        }
        int expected = imap_v[k];
        int got = map[n];
        if (got != expected)
        {
            std::wstringstream ss;
            ss << L"reverse map mismatch at k=" << k
               << L": map[imap_k[k]] = map[" << n << L"]=" << got
               << L" but imap_v[k]=" << expected;
            fail(ss.str());
        }
    }

    if (verbose && ok) std::wcout << L"[OK] " << label << L"\n";
    return ok;
}

template<typename T>
inline bool validate_kv_map(const wchar_t* label, const kv_map<T>& m, bool verbose = true)
{
    bool ok = true;

    if (m.len > 0 && !m.items)
    {
        ok = false;
        if (verbose) std::wcout << L"[FAIL] " << label << L": items is null (len>0)\n";
    }

    bool ok_maps = validate_kv_map_common(label, m.map, m.imap_v, m.imap_k, m.len, verbose);
    return ok && ok_maps;
}

template<typename T, typename PrintFn>
inline void dump_kv_map(
    const wchar_t* label,
    const kv_map<T>& m,
    PrintFn print_item,
    int max_items = 8)
{
    std::wcout << L"\n=== " << label << L" ===\n";
    std::wcout << L"len=" << m.len
               // << L" items=" << (void*)m.items
               // << L" map=" << (void*)m.map
               // << L" imap_v=" << (void*)m.imap_v
               // << L" imap_k=" << (void*)m.imap_k
               << L"\n";

    if (m.len <= 0 || !m.items) return;

    int count = (max_items < 0 || max_items > m.len) ? m.len : max_items;
    for (int i = 0; i < count; i++)
        print_item(m.items[i], i);

    if (count < m.len)
        std::wcout << L"  ... (" << (m.len - count) << L" more)\n";

    // Show map arrays (first few)
    int show = std::min(m.len, 12);
    std::wcout << L"map:    ";
    for (int i = 0; i < show; i++) std::wcout << m.map[i] << L" ";
    if (show < m.len) std::wcout << L"...";
    std::wcout << L"\n";

    std::wcout << L"imap_v: ";
    for (int i = 0; i < show; i++) std::wcout << m.imap_v[i] << L" ";
    if (show < m.len) std::wcout << L"...";
    std::wcout << L"\n";

    std::wcout << L"imap_k: ";
    for (int i = 0; i < show; i++) std::wcout << m.imap_k[i] << L" ";
    if (show < m.len) std::wcout << L"...";
    std::wcout << L"\n";
}

inline bool validate_renderables(const renderables& r, bool verbose = true)
{
    bool ok = true;

    ok &= validate_kv_map(L"entities", r.entities, verbose);
    ok &= validate_kv_map(L"objects", r.objects, verbose);
    ok &= validate_kv_map(L"cameras", r.cameras, verbose);
    ok &= validate_kv_map(L"float1s", r.float1s, verbose);
    ok &= validate_kv_map(L"float3s", r.float3s, verbose);
    ok &= validate_kv_map(L"strings", r.strings, verbose);

    if (verbose)
        std::wcout << (ok ? L"\nRenderables validation PASSED\n"
                          : L"\nRenderables validation FAILED\n");
    return ok;
}

//inline
void dump_renderables(const renderables& r)
{
    dump_renderables(r, 8);
}

void dump_renderables(const renderables& r, int max_items)
{
    dump_kv_map(L"entities", r.entities, print_entity, max_items);
    dump_kv_map(L"objects", r.objects, print_object, max_items);
    dump_kv_map(L"materials", r.materials, print_material, max_items);
    dump_kv_map(L"lights", r.lights, print_light, max_items);
    dump_kv_map(L"cameras", r.cameras, print_camera, max_items);
    dump_kv_map(L"float1s", r.float1s, print_xml_ir_float1, max_items);
    dump_kv_map(L"float3s", r.float3s, print_xml_ir_float3, max_items);
    dump_kv_map(L"strings", r.strings, print_xml_ir_string, max_items);
}
