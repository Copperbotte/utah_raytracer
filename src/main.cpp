
#include <iostream>
#include <fcntl.h>
#include <Windows.h>
#include <io.h>

#include "xml.hpp"
#include "xml_compiler.hpp"

#include <SDL3/SDL.h>
#include <cstdio>


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

    //return sdl_test_01();
    //return sdl_test_02();
    //return sdl_test_03();

    //decode_xml_components(components);
    renderables Renderables;
    init_renderables(Renderables, components);
    dump_renderables(Renderables, /*max_items=*/16);



    return 0;
}




