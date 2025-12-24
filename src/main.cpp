
#include <iostream>
#include <fcntl.h>
#include <Windows.h>
#include <io.h>

#include <math.h>
#include <array>

#include "xml.hpp"
#include "xml_compiler.hpp"

#include <SDL3/SDL.h>
#include <cstdio>

#include "einsum_variadic_ct.hpp"
#include "vec.hpp"


int sdl_test_01(){
    if(!SDL_Init(SDL_INIT_VIDEO)) {
        std::printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    std::printf("SDL initialized successfully.\n");
    SDL_Quit();
    return 0;
}

int sdl_test_02(){
    if(!SDL_Init(SDL_INIT_VIDEO)) {
        std::printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("SDL Test Window",
        800, 600,
        SDL_WINDOW_RESIZABLE
    );

    if (!window) {
        std::printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    bool running = true;
    while(running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }
        SDL_Delay(16); // Roughly 60 FPS
    }

    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

int sdl_test_03(){
    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window* window = SDL_CreateWindow("Framebuffer Test", 512, 512,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY
    );
    //SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL, 0);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);
    // PFD_GENERIC_ACCELERATED?
    // 


    SDL_Texture* texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_STREAMING,
        512, 512
    );

    
    std::vector<uint32_t> pixels(512 * 512);
    for (int y = 0; y < 512; ++y)
    {
        for (int x = 0; x < 512; ++x)
        {
            uint8_t r = x & 0xFF;
            uint8_t g = y & 0xFF;
            uint8_t b = (64*((x / 256 + y / 256) % 4) - 1) & 0xFF;
            uint8_t a = 255;
            pixels[y * 512 + x] = (r << 24) | (g << 16) | (b << 8) | a;
        }
    }

    SDL_UpdateTexture(texture, nullptr, pixels.data(), 512 * sizeof(uint32_t));

    bool running = true;
    while (running)
    {
        SDL_Event e;
        while (SDL_PollEvent(&e))
            if (e.type == SDL_EVENT_QUIT)
                running = false;

        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

int sdl_display_bbuffer(std::vector<float> &backbuffer, int w, int h){
    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window* window = SDL_CreateWindow("Framebuffer Test", w, h,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY
    );
    //SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL, 0);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);
    // PFD_GENERIC_ACCELERATED?
    // 


    SDL_Texture* texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_STREAMING,
        w, h
    );

    float lo = 1e30f;
    float hi = -1e30f;
    for(const float &f : backbuffer)
    {
        lo = std::min(lo, f);
        hi = std::max(hi, f);
    }
    std::cout << "lo = " << lo << " hi " << hi << std::endl;
    
    if (hi <= lo) {
        // avoid division by zero / degenerate range — set to mid-gray
        for(float &f : backbuffer)
            f = 127.0f;
    } else {
        for(float &f : backbuffer)
            f = 255.0f*(f - lo)/(hi - lo);
    }

    std::vector<uint32_t> pixels(w*h);
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            uint8_t r = uint8_t(backbuffer[(w*y + x)*3 + 0]);
            uint8_t g = uint8_t(backbuffer[(w*y + x)*3 + 1]);
            uint8_t b = uint8_t(backbuffer[(w*y + x)*3 + 2]);
            uint8_t a = 255;
            pixels[y * w + x] = (r << 24) | (g << 16) | (b << 8) | a;
        }
    }

    SDL_UpdateTexture(texture, nullptr, pixels.data(), w * sizeof(uint32_t));

    bool running = true;
    while (running)
    {
        SDL_Event e;
        while (SDL_PollEvent(&e))
            if (e.type == SDL_EVENT_QUIT)
                running = false;

        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

// std::vector is a dynamic array with no operations defined,
// std::array is a fixed array *at compile time* with no operations defined,
// std::valarray is a dynamic array with operations defined, but lacks the ability to get the underlying data??
// Confusing. Shocked this isn't one set of objects.
// template<std::size_t N>
// float dot(const std::array<float,N> &a, const std::array<float,N> &b)
// {
//     float res = 0.0f;
//     for(std::size_t n=0; n<a.size(); n++)
//         res += a[n]*b[n];
//     return res;
// }
// template<std::size_t N>
// float mag(const std::array<float,N> &a)
// {
//     return sqrt(dot<N>(a,a)); // Guaranteed to never be < 0.
// }
// template<std::size_t N>
// std::array<float,N> normalize(const std::array<float,N> &a)
// {
//     std::array<float,N> res = a;
//     float fmag = mag<N>(a);
//     if(fmag == 0.0f)
//         return a;
//     for(std::size_t n=0; n<a.size(); n++)
//         res[n] = a[n]/fmag;
//     return res;
// }

//template<class T, class Labels, class Shape>
//constexpr tensor<const T, Labels, Shape> make_tensor(const T* ptr) { return {ptr}; }
/*
template<class T, std::size_t N, int... Ls, std::size_t... Ds>
constexpr auto as_tensor(const vec<T,N>& v, labels_t<Ls...>, shape_t<Ds...>)
{
    static_assert(((Ds * ... * std::size_t{1}) == N),
                  "as_tensor: shape product must equal vec length");
    return make_tensor<const T, labels_t<Ls...>, shape_t<Ds...>>(v.data());
}

template<class T, std::size_t N, int... Ls, std::size_t... Ds>
constexpr auto as_tensor(vec<T,N>& v, labels_t<Ls...>, shape_t<Ds...>)
{
    static_assert(((Ds * ... * std::size_t{1}) == N),
                  "as_tensor: shape product must equal vec length");
    return make_tensor<T, labels_t<Ls...>, shape_t<Ds...>>(v.data());
}
*/
vec<float,3> mul(vec<float,9>& M, vec<float,3>& x)
{
    vec<float,3> y{};
    auto Mt = make_tensor<labels_t<0,1>, shape_t<3,3>>(M.data());
    auto xt = make_tensor<labels_t<1>,   shape_t<3>>(x.data());
    
    //auto Mt = as_tensor(M, labels_t<0,1>{}, shape_t<3,3>{}); // i,j
    //auto xt = as_tensor(x, labels_t<1>{},   shape_t<3>{});   // j
    einsum_into<0>(y.data(), Mt, xt);                        // -> i
    return y;
}



using namespace std;
int main(int argc, wchar_t** argv) {
    _setmode(_fileno(stdout), _O_U16TEXT);

    //std::array<float,3> fuck;
    //fuck.data

    //std::wstring path =  L"./scenes/scene_00.xml";
    std::wstring path =  L"./scenes/scene_01.xml";

    std::vector<xml_component> components;
    std::wstring result = read_xml(components, path);
    if (result != L"")
    {
        wcout << "Error reading XML: " << result << endl;
        return -1;
    }
    wcout << pprint_components(components);

    //return sdl_test_01();
    //return sdl_test_02();
    //return sdl_test_03();

    //decode_xml_components(components);
    renderables Renderables;
    init_renderables(Renderables, components);
    dump_renderables(Renderables, /*max_items=*/16);
    
    const camera &cam = Renderables.cameras[0];
    std::vector<float> backbuffer(std::size_t(cam.h*cam.w*3), 0.0f);
    const float π = 3.141592653589793; // via mathematica
    float θ = cam.fov_deg * (π / 180.0f) / 2.0f;
    float zs = atan(θ);
    float ws = max(1.0f, float(cam.w)/float(cam.h)) * zs;
    float hs = max(1.0f, float(cam.h)/float(cam.w)) * zs;

    //std::array<float,3> scale{ws, hs, 1.0f};
    //std::array<float,3> pos{0.0f};
    //std::array<float,3> dir{0.0f};

    typedef vec<float,3> vec3;
    vec3 scale{ws, hs, 1.0f};
    vec3 pos = vec3(cam.pos, 3);
    vec3 dir{0.0f};
    vec3 target = vec3(cam.target, 3);

    // Find the view matrix
    vec3 cam_zh = normalize(target - pos);
    // Grahm-schmidt the up vector
    vec3 cam_yh = vec3(cam.up, 3);
    cam_yh = normalize(cam_yh - dot(cam_yh, cam_zh));
    vec3 cam_xh = cross(cam_zh, cam_yh);

    using mat3 = vec<float,9>; // row-major [i*3 + j]
    mat3 R = {
        cam_xh[0], cam_yh[0], cam_zh[0],
        cam_xh[1], cam_yh[1], cam_zh[1],
        cam_xh[2], cam_yh[2], cam_zh[2],
    };
    
    for(int iv=0; iv<cam.h; iv++)
    {
        float v = (float(iv) + 0.5f)/cam.h; // Adds 0.5f so the pixel is centered
        v = v*2.0f - 1.0f;
        v *= -hs;
        for(int iu=0; iu<cam.w; iu++)
        {
            float u = (float(iu) + 0.5f)/cam.w;
            u = u*2.0f - 1.0f;
            u *= ws;
            
            dir[0] = u;
            dir[1] = v;
            dir[2] = 1.0f;
            //dir[1] = 1.0f;
            //dir[2] = v;
            // Normalize dir
            dir = normalize(dir);
            dir = mul(R, dir);

            // Ray-sphere intersection: find nearest hit (if any)
            float t_min = 1e30f;
            bool hit = false;
            vec3 hit_normal{0.0f};
            float color = 0.0f;

            //for(const auto &sph : Renderables.spheres.items)
            for(int nsph=0; nsph<Renderables.spheres.len; nsph++)
            {
                sphere &sph = Renderables.spheres.items[nsph];

                vec3 spos = vec3(sph.pos, 3);
                float radius = sph.radius; // adjust field name if your sphere uses a different name

                // Ascend parents if any
                // lazy
                
                if(sph.parent >= 0)
                {
                    spos = vec3(Renderables.spheres.items[1].pos,3) + spos*Renderables.spheres.items[1].radius;
                    radius *= Renderables.spheres.items[1].radius;
                }

                vec3 oc = pos - spos;
                // Solve t^2 + 2*b*t + c = 0  where b = dot(oc,dir) and c = dot(oc,oc) - r^2
                float b = dot(oc, dir);
                float c = dot(oc, oc) - radius*radius;
                float disc = b*b - c;
                if (disc < 0.0f) continue; // no real roots -> miss

                float sq = sqrtf(disc);
                float t0 = -b - sq;
                float t1 = -b + sq;

                // pick nearest positive t (with a small epsilon to avoid self-intersection)
                const float EPS = 1e-4f;
                float t = (t0 > EPS) ? t0 : ((t1 > EPS) ? t1 : -1.0f);
                if (t > 0.0f && t < t_min) {
                    t_min = t;
                    hit = true;
                    vec3 hitpos = pos + dir * t;
                    hit_normal = normalize(hitpos - spos);
                    color += 1.0f;
                }
            }

            if (hit) {
                // Simple shading: map normal [-1,1] -> [0,1] as color
                dir = hit_normal * 0.5f + vec3{0.5f};
            } else {
                // background color (e.g., sky gradient)
                dir = vec3{0.2f, 0.3f, 0.6f}; // tweak as desired
            }


            backbuffer[(cam.w*iv + iu)*3 + 0] = dir[0];
            backbuffer[(cam.w*iv + iu)*3 + 1] = dir[1];
            backbuffer[(cam.w*iv + iu)*3 + 2] = dir[2];
        }
    }

    sdl_display_bbuffer(backbuffer, cam.w, cam.h);
    

    return 0;
}




