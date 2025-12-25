
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

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
#include <bit>
#include <cstdint>
#include <cmath>
#include <chrono>

unsigned long xorshfnums[3] = {123456789, 362436069, 521288629}; // I think these are random, and can be randomized using a seed
unsigned long xorshift96(void) // YOINK http://stackoverflow.com/questions/1640258/need-a-fast-random-generator-for-c
{          //period 2^96-1
	unsigned long t;
	xorshfnums[0] ^= xorshfnums[0] << 16;
	xorshfnums[0] ^= xorshfnums[0] >> 5;
	xorshfnums[0] ^= xorshfnums[0] << 1;

	t = xorshfnums[0];
	xorshfnums[0] = xorshfnums[1];
	xorshfnums[1] = xorshfnums[2];
	xorshfnums[2] = t ^ xorshfnums[0] ^ xorshfnums[1];

	return xorshfnums[2];
}
double xorshiftdbl(void)
{ // Double: sign bit, 11 exponent bits, 52 fraction bits,  0x3ff0000000000000 = Exponent and Power section, equivelant to 1
	std::uint64_t x = 0x3ff0000000000000ull | (std::uint64_t(xorshift96()) << 20); //xorshift92 is 32 bits long, 32 - 52 = 20 bits shifted
	return std::bit_cast<double>(x) - 1.0;
}
float xorshiftflt(void)
{ // Float: sign bit, 8 exponent bits, 23 fraction bits. 0x3f800000 = 1.0f
	std::uint32_t x = 0x3f800000u | (std::uint32_t(xorshift96()) >> 9); // use top 23 bits from xorshift96
	return std::bit_cast<float>(x) - 1.0f;//*(float*)&x - 1.0f;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

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

    /*
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
    */

    for(float &f : backbuffer)
        f = std::min(255.0f, std::max(0.0f, f*255.0f));

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
template<size_t N>
vec<float,N> mul(const vec<float,N*N>& M, const vec<float,N>& x)
{
    vec<float,N> y{};
    auto Mt = make_tensor<labels_t<0,1>, shape_t<N,N>>(M.data());// i,j
    auto xt = make_tensor<labels_t<1>,   shape_t<N>>(  x.data());// j
    einsum_into<0>(y.data(), Mt, xt);                            // -> i
    return y;
}

template<size_t N>
vec<float,N> mul(const vec<float,N>& x, const vec<float,N*N>& M)
{
    vec<float,N> y{};
    auto Mt = make_tensor<labels_t<1,0>, shape_t<N,N>>(M.data());// i,j
    auto xt = make_tensor<labels_t<1>,   shape_t<N>>(x.data());  // j
    einsum_into<0>(y.data(), xt, Mt);                        // -> i
    return y;
}

vec<float,3> mul3_affine(const vec<float,16>& M, const vec<float,3>& x, float w)
{
    vec<float,4> y{};
    vec<float,4> x2(x.array);
    x2[3] = w;
    x2 = mul(M, x2);
    vec<float,3> y2(x2.array);
    return y2;
}

vec<float,3> mul3_affine(const vec<float,3>& x, float w, const vec<float,16>& M)
{
    vec<float,4> y{};
    vec<float,4> x2(x.array);
    x2[3] = w;
    x2 = mul(x2, M);
    vec<float,3> y2(x2.array);
    return y2;
}

vec<float,16> mul44_44(const vec<float,16>& L, const vec<float,16>& R)
{
    vec<float,16> y{};
    auto Lt = make_tensor<labels_t<0,2>, shape_t<4,4>>(L.data());
    auto Rt = make_tensor<labels_t<2,1>, shape_t<4,4>>(R.data());
    einsum_into<0,1>(y.data(), Lt, Rt);
    return y;
}

template<size_t N> //constexpr // If vec isn't constexpr-constructible, remove constexpr.
vec<float,N*N> identity()
{
    vec<float,N*N> res{0.0f};
    for(size_t n=0; n<N*N; n+=N+1)
        res[n] = 1.0f;
    return res;
}

static inline vec<float,16> translate4(const vec<float,3> dx)
{
    vec<float,16> T = identity<4>();
    // row-major, last column
    T.array[ 3] = dx[0];
    T.array[ 7] = dx[1];
    T.array[11] = dx[2];
    return T;
}

static inline vec<float,16> scale4(const vec<float,3> s)
{
    vec<float,16> S = identity<4>();
    S.array[ 0] = s[0];
    S.array[ 5] = s[1];
    S.array[10] = s[2];
    return S;
}

// I really ought to rewrite this as a GA bivector rotation to ignore degenerate axes.
// Lets handle that case in rotate4 and assume axis is a unit.
vec<float,3> rodrigues(const vec<float,3> x, const vec<float,3> axis, float angle)
{
    float ct = std::cos(angle);
    float st = std::sin(angle);

    vec<float,3> y = axis * dot(axis, x);
    vec<float,3> z = cross(axis, x);

    return x*ct + z*st + y*(1.0f - ct);
}

static inline vec<float,16> rotate4(const vec<float,3> axis, float angle)
{
    vec<float, 3> ax;// = axis;
    if(dot(axis,axis) < 1e-4f) ax = vec<float,3>{1,0,0};
    else ax = normalize(axis);

    vec<float,3> ex = rodrigues(vec<float,3>{1,0,0}, ax, angle);
    vec<float,3> ey = rodrigues(vec<float,3>{0,1,0}, ax, angle);
    vec<float,3> ez = rodrigues(vec<float,3>{0,0,1}, ax, angle);

    vec<float,16> R = identity<4>();
    R.array[0] = ex[0]; R.array[1] = ey[0]; R.array[2 ] = ez[0];
    R.array[4] = ex[1]; R.array[5] = ey[1]; R.array[6 ] = ez[1];
    R.array[8] = ex[2]; R.array[9] = ey[2]; R.array[10] = ez[2];
    return R;
}

// local model->parent: L = T * S  (scale then translate)
static inline vec<float,16> local_model_to_parent(const object& sph)
{
    vec<float,16> T = translate4(vec<float,3>(sph.pos,3));
    vec<float,16> R = rotate4(vec<float,3>(sph.rotation,3), sph.rotation[3]);
    vec<float,16> S = scale4(vec<float,3>(sph.scale,3));

    vec<float,16> res{};
    auto Tt = make_tensor<labels_t<0,1>, shape_t<4,4>>(T.data());
    auto Rt = make_tensor<labels_t<1,2>, shape_t<4,4>>(R.data());
    auto St = make_tensor<labels_t<2,3>, shape_t<4,4>>(S.data());
    einsum_into<0,3>(res.data(), Tt, Rt, St);
    return res;
}

// inverse: (T*S)^-1 = S^-1 * T^-1
static inline vec<float,16> local_parent_to_model(const object& sph)
{
    vec<float,3> t = vec<float,3>(sph.pos,3);
    vec<float,3> s = vec<float,3>(sph.scale,3);
    vec<float,3> is;
    for(int n=0;n<3;n++)
        is[n] = (s[n] != 0.0f) ? 1.0f/s[n] : 0.0f;

    vec<float,16> iS = scale4(is);
    vec<float,16> iR = rotate4(vec<float,3>(sph.rotation,3), -sph.rotation[3]);
    vec<float,16> iT = translate4(-t);

    vec<float,16> res{};
    auto iTt = make_tensor<labels_t<2,3>, shape_t<4,4>>(iT.data());
    auto iRt = make_tensor<labels_t<1,2>, shape_t<4,4>>(iR.data());
    auto iSt = make_tensor<labels_t<0,1>, shape_t<4,4>>(iS.data());
    einsum_into<0,3>(res.data(), iSt, iRt, iTt);
    return res;

    //return mul44_44(iS, iT);
}

struct traverse_result
{
    float dist = 1e30f;
    int hit = -1;
    vec<float,3> hit_normal{0.0f};
};

vec<float,3> normal_world_from_model(const vec<float,3>& nM, const vec<float,16>& iW)
//                             const std::vector<vec<float,16>>& object_mdl_from_world)
{
    // = object_mdl_from_world[nsph]; // model_from_world = W^{-1}

    vec<float,3> nW{};
    // nW = (iW_3x3)^T * nM   (note: transpose!)
    nW[0] = iW[0]*nM[0] + iW[4]*nM[1] + iW[8]*nM[2];
    nW[1] = iW[1]*nM[0] + iW[5]*nM[1] + iW[9]*nM[2];
    nW[2] = iW[2]*nM[0] + iW[6]*nM[1] + iW[10]*nM[2];
    return normalize(nW);
}


traverse_result traverse(const vec<float,3> &pos, const vec<float,3> &dir0,
    std::vector<vec<float,16>> &object_world_from_mdl,
    std::vector<vec<float,16>> &object_mdl_from_world,
    renderables &Renderables,
    int src_id=-2
){
    typedef vec<float,3> vec3;

    vec<float,3> dir = normalize(dir0);

    float t_min = 1e30f;
    int hit = -1;
    vec3 hit_normal{0.0f};

    for(int nsph=0; nsph<Renderables.objects.len; nsph++)
    //for(int nsph=0; nsph<1; nsph++)
    {
        if(src_id == Renderables.objects.items[nsph].entity)
            continue;
        
        if(Renderables.objects.items[nsph].type != L"sphere")
            continue;

        vec3 p = mul3_affine(object_mdl_from_world[nsph], pos, 1);
        vec3 d = mul3_affine(object_mdl_from_world[nsph], dir, 0);

        // Solve t^2 + 2*b*t + c = 0  where b = dot(oc,dir) and c = dot(oc,oc) - r^2
        //float b = dot(p, d);
        //float c = dot(p, p) - 1.0f;
        //float disc = b*b - c;
        float a = dot(d, d);
        float b = dot(p, d);
        float c = dot(p, p) - 1.0f;
        float disc = b*b - a*c;
        if (disc < 0.0f) continue; // no real roots -> miss

        float sq = sqrtf(disc);
        float t0 = (-b - sq)/a;
        float t1 = (-b + sq)/a;

        // pick nearest positive t (with a small epsilon to avoid self-intersection)
        const float EPS = 1e-4f;
        float t = (t0 > EPS) ? t0 : ((t1 > EPS) ? t1 : -1.0f);
        if (t > 0.0f){// && t < t_min) {
            // t_min = t;
            // hit = true;
            p += d*t;
            vec3 nrml = normalize(p);// + p;

            p = mul3_affine(object_world_from_mdl[nsph], p, 1);
            //nrml = mul3_affine(nrml, 0, object_world_from_mdl[nsph]);
            nrml = mul3_affine(nrml, 0, object_mdl_from_world[nsph]);
            //nrml = mul3_affine(object_world_from_mdl[nsph], nrml, 0);
            //nrml = normal_world_from_model(nrml, object_world_from_mdl[nsph]);
            //nrml = mul3_affine(object_world_from_mdl[nsph], nrml, 1);
            //nrml = normalize(nrml - p);

            //nrml = normal_world_from_model(nrml, object_mdl_from_world[nsph]);


            // float t2 = dot(p-pos,p-pos);
            // if(t2 < t_min*t_min)
            // {
            //     hit = Renderables.objects[nsph].entity;
            //     t_min = std::sqrt(t2);
            //     hit_normal = normalize(nrml);
            // }

            float tW = dot(p - pos, dir);  // dir must be normalized
            //if (tW > EPS && tW < t_min) {
            if (tW < t_min) {
                t_min = tW;
                hit = Renderables.objects.items[nsph].entity;
                hit_normal = nrml;
            }


        }
    }

    traverse_result result;
    result.hit = hit;
    result.dist = t_min;
    result.hit_normal = normalize(hit_normal);
    return result;
}

vec<float,3> phong(
    const vec<float,3> &view,
    const vec<float,3> &Light,
    const vec<float,3> &normal,
    const vec<float,3> &albedo,
    const vec<float,3> &spec_color,
    const vec<float,3> &spec_power,
    const vec<float,3> &metalness
){
    typedef vec<float,3> vec3;
    vec3 nL = normalize(Light);
    vec3 R = nL - normal*2.0f*dot(nL,normal);
    float lambert = std::max(0.0f, dot(nL, normal));
    float vdr = std::max(0.0f, dot(view, R));
    vec3 phong_spec = pow(vec3(vdr), spec_power);
    vec3 diffuse = albedo * lambert;
    vec3 spec = spec_color * phong_spec * (spec_power+2.0f)/2.0f;
    return mag(Light)*lerp(diffuse, spec, metalness);
}

vec<float,3> blinn_phong(
    const vec<float,3> &view,
    const vec<float,3> &Light,
    const vec<float,3> &normal,
    const vec<float,3> &albedo,
    const vec<float,3> &spec_color,
    const vec<float,3> &spec_power,
    const vec<float,3> &metalness
){
    typedef vec<float,3> vec3;

    vec3 nL = normalize(Light);
    vec3 V  = -normalize(view);
    vec3 N  = normalize(normal);

    // Blinn-Phong uses the half-vector H = normalize(L + V)
    vec3 H = nL + V;
    float hlen = mag(H);
    if (hlen > 0.0f)
        H = H * (1.0f / hlen);
    else
        H = V; // fallback to avoid NaNs when L == -V

    float lambert = std::max(0.0f, dot(nL, N));
    float nDotH   = std::max(0.0f, dot(N, H));

    // specular term using Blinn-Phong (raise n·h to specular power)
    vec3 blinn_spec = pow(vec3(nDotH), spec_power);
    vec3 diffuse = albedo * lambert;

    vec3 bs_normalizer = (spec_power+2.0f)*(spec_power+4.0f)/(8.0f*3.141592f*(pow(2.0f, -spec_power/2.0f) + spec_power));
    //vec3 bs_normalizer = (spec_power+8.0f)/8.0f;
    vec3 spec = spec_color * blinn_spec * bs_normalizer;// * (spec_power + 2.0f) / 2.0f;

    return mag(Light)*lerp(diffuse, spec, metalness);
}

// GLSL-inspired RGB<->HSV helpers adapted to the project's vec<T,N> types.
// Source algorithm originally from: https://stackoverflow.com/questions/15095909/from-rgb-to-hsv-in-opengl-glsl
// Implemented using vec<float,3> and small helpers from vec.hpp.
vec<float,3> rgb2hsv(const vec<float,3>& rgb)
{
    // rgb components in [0,1]
    float r = rgb[0];
    float g = rgb[1];
    float b = rgb[2];

    float mx = std::max(r, std::max(g, b));
    float mn = std::min(r, std::min(g, b));
    float d = mx - mn;

    float h = 0.0f;
    if (d > 1e-10f) {
        if (mx == r) {
            h = fmodf((g - b) / d, 6.0f);
        } else if (mx == g) {
            h = (b - r) / d + 2.0f;
        } else {
            h = (r - g) / d + 4.0f;
        }
        h /= 6.0f; // normalize to [0,1)
        if (h < 0.0f) h += 1.0f;
    }

    float s = (mx <= 1e-10f) ? 0.0f : (d / mx);
    float v = mx;

    return vec<float,3>{h, s, v};
}

vec<float,3> hsv2rgb(const vec<float,3>& hsv)
{
    float h = hsv[0];
    float s = hsv[1];
    float v = hsv[2];

    // h in [0,1), scale to [0,6)
    float hh = h * 6.0f;
    int i = int(std::floor(hh)) % 6; // sector 0..5
    float f = hh - std::floor(hh);

    float p = v * (1.0f - s);
    float q = v * (1.0f - s * f);
    float t = v * (1.0f - s * (1.0f - f));

    switch (i) {
        case 0: return vec<float,3>{v, t, p};
        case 1: return vec<float,3>{q, v, p};
        case 2: return vec<float,3>{p, v, t};
        case 3: return vec<float,3>{p, q, v};
        case 4: return vec<float,3>{t, p, v};
        case 5: default: return vec<float,3>{v, p, q};
    }
}

vec<float,3> saturationClip(const vec<float,3>& rgb)
{
    vec<float,3> hsv = rgb2hsv(rgb);

    // If v (value) exceeds 1.0, scale down saturation and value so v==1
    if (hsv[2] > 1.0f) {
        float scale = 1.0f / hsv[2];
        hsv[1] *= scale;
        hsv[2] = 1.0f;
    }

    return hsv2rgb(hsv);
}

template<typename T, std::size_t N>
void print_vec(const std::wstring &name, const vec<T,N> &v)
{
    std::wcout << name;
    std::wprintf(L":");
    for (int c = 0; c < N; ++c)
    {
        if(c+1 != N)
            std::wprintf(L"%10.4f, ", static_cast<double>(v[c]));
        else
            std::wprintf(L"%10.4f ", static_cast<double>(v[c]));
    }
    std::wprintf(L"\n");
}

int main(int argc, wchar_t** argv) {
    _setmode(_fileno(stdout), _O_U16TEXT);

    //std::array<float,3> fuck;
    //fuck.data

    //std::wstring path =  L"./scenes/scene_00.xml";
    //std::wstring path =  L"./scenes/scene_01.xml";
    //std::wstring path =  L"./scenes/project_2_scene.xml";
    std::wstring path =  L"./scenes/project_3_scene.xml";

    std::vector<xml_component> components;
    std::wstring result = read_xml(components, path);
    if (result != L"")
    {
        std::wcout << "Error reading XML: " << result << std::endl;
        return -1;
    }
    std::wcout << pprint_components(components);

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
    float zs = std::atan(θ);
    float ws = std::max(1.0f, float(cam.w)/float(cam.h)) * zs;
    float hs = std::max(1.0f, float(cam.h)/float(cam.w)) * zs;

    //std::array<float,3> scale{ws, hs, 1.0f};
    //std::array<float,3> pos{0.0f};
    //std::array<float,3> dir{0.0f};

    typedef vec<float,3> vec3;
    vec3 scale{ws, hs, 1.0f};
    vec3 pos = vec3(cam.pos, 3);
    vec3 dir{0.0f};
    vec3 target = vec3(cam.target, 3);


    print_vec(L"pos", pos);
    print_vec(L"target", target);

    // Find the view matrix
    vec3 cam_zh = target - pos;
    print_vec(L"cam_zh", cam_zh);
    cam_zh = normalize(cam_zh);
    print_vec(L"cam_zh", cam_zh);

    // Grahm-schmidt the up vector
    vec3 cam_yh = vec3(cam.up, 3);
    cam_yh = normalize(cam_yh - cam_zh * dot(cam_zh, cam_yh));
    vec3 cam_xh = cross(cam_zh, cam_yh);

    using mat3 = vec<float,9>; // row-major [i*3 + j]
    mat3 View_tf = {
        cam_xh[0], cam_yh[0], cam_zh[0],
        cam_xh[1], cam_yh[1], cam_zh[1],
        cam_xh[2], cam_yh[2], cam_zh[2],
    };


    print_vec(L"cam_zh", cam_zh);
    print_vec(L"cam_yh", cam_yh);
    print_vec(L"cam_xh", cam_xh);

    std::wprintf(L"View_tf:\n");
        for (int r = 0; r < 3; ++r) {
            std::wprintf(L"  ");
            for (int c = 0; c < 3; ++c) {
                std::wprintf(L"%10.4f ", static_cast<double>(View_tf[r*3 + c]));
            }
            std::wprintf(L"\n");
        }
        std::wprintf(L"\n");
    
    using mat4 = vec<float,16>;
    using vec4 = vec<float,4>;

    std::vector<mat4> object_world_from_mdl;
    std::vector<mat4> object_mdl_from_world;

    object_world_from_mdl.resize(Renderables.objects.len);
    object_mdl_from_world.resize(Renderables.objects.len);

    for (int nsph = 0; nsph < Renderables.objects.len; nsph++)
    {
        object &start = Renderables.objects.items[nsph];

        mat4 W  = identity<4>();
        mat4 iW = identity<4>();

        int eid = start.entity;
        //std::wcout << L"eid:" << eid << L" pid:" << cur.parent << L", ";
        while (0 <= eid)
        {
            
            int idx = search_sorted_index(Renderables.objects.imap_v,
                                        Renderables.objects.len,
                                        eid);
            std::wcout << L"idx:" << idx << L" ";
            
            if (idx < 0) break;
            //if (eid < 0) break;

            object &cur = Renderables.objects.items[idx];

            mat4 L  = local_model_to_parent(cur);
            mat4 iL = local_parent_to_model(cur);

            W  = mul44_44(L, W);     // prepend
            iW = mul44_44(iW, iL);   // append (this is the key!)

            eid = cur.parent;
            std::wcout << L"eid:" << eid << L" ";
        }
        std::wcout << std::endl;

        // Pretty-print W and iW (row-major 4x4)
        std::wprintf(L"object %d (entity %d) world_from_model (W):\n", nsph, start.entity);
        for (int r = 0; r < 4; ++r) {
            std::wprintf(L"  ");
            for (int c = 0; c < 4; ++c) {
                std::wprintf(L"%10.4f ", static_cast<double>(W[r*4 + c]));
            }
            std::wprintf(L"\n");
        }
        std::wprintf(L"object %d model_from_world (iW):\n", nsph);
        for (int r = 0; r < 4; ++r) {
            std::wprintf(L"  ");
            for (int c = 0; c < 4; ++c) {
                std::wprintf(L"%10.4f ", static_cast<double>(iW[r*4 + c]));
            }
            std::wprintf(L"\n");
        }
        std::wprintf(L"\n");

        object_world_from_mdl[nsph] = W;
        object_mdl_from_world[nsph] = iW;
    }

    // Find the ambient term early
    vec3 ambient = vec3{0.2f, 0.3f, 0.6f};
    for(std::size_t n=0; n<Renderables.lights.len; n++)
    {
        if(Renderables.lights[n].type == L"ambient")
        {
            ambient = vec3(Renderables.lights[n].intensity, 3);
            break;
        }
    }
    ambient = pow(ambient, vec3{2.2});

    const int spp_lim = 16;
    for(int spp=0; spp<spp_lim; spp++)
    {

    for(int iv=0; iv<cam.h; iv++)
    {
        // float v = (float(iv) + 0.5f)/cam.h; // Adds 0.5f so the pixel is centered
        // v = v*2.0f - 1.0f;
        // v *= -hs;
        for(int iu=0; iu<cam.w; iu++)
        {
            float v = (xorshiftflt() + float(iv) + 0.5f)/cam.h; // Adds 0.5f so the pixel is centered
            v = v*2.0f - 1.0f;
            v *= -hs;

            float u = (xorshiftflt() + float(iu) + 0.5f)/cam.w;
            u = u*2.0f - 1.0f;
            u *= ws;

            dir = vec3{u,v,1.0f};
            dir = normalize(dir);
            dir = normalize(mul(View_tf, dir));

            // Ray-sphere intersection: find nearest hit (if any)
            float t_min = 1e30f;
            int hit = false;
            vec3 hit_normal{0.0f};
            vec3 color = ambient;//vec3{0.0f};

            traverse_result tv_res =  traverse(pos, dir, object_world_from_mdl, 
                object_mdl_from_world, Renderables);

            hit = tv_res.hit;
            hit_normal = tv_res.hit_normal;
            t_min = tv_res.dist;

            if (0 <= hit) do {
                // Simple shading: map normal [-1,1] -> [0,1] as color
                //dir = hit_normal * 0.5f + vec3{0.5f};

                vec3 hit_pos = pos + dir*t_min;

                // Find object by entitiy id.
                int sid = search_sorted_index(Renderables.objects.imap_v, Renderables.objects.len, hit);
                if(sid < 0)
                {
                    hit = -1;
                    break;
                }
                int mid = Renderables.objects.items[Renderables.objects.imap_k[sid]].mid;
                if(mid < 0)
                {
                    hit = -1;
                    break;
                }
                material &mat = Renderables.materials.items[mid];

                vec3 albedo = pow(vec3(mat.albedo,3),vec3{2.2});
                vec3 spec_color = pow(vec3(mat.spec_color,3),vec3{2.2});
                vec3 glossiness = vec3(mat.glossiness,3);// * 100.0f;
                vec3 metalness = vec3(mat.glossiness_value);//vec3(1.0 - mat.glossiness_value);
                vec3 view = normalize(dir);

                // Add on phong filtered ambient
                color = vec3{0};//ambient*(albedo*(1.0-metalness) + metalness*spec_color);

                for(int lid=0; lid < Renderables.lights.len; lid++)
                {
                    light &Light = Renderables.lights[lid];
                    vec3 LLum = pow(vec3(Light.intensity,3), vec3{2.2});
                    if(Light.type == L"ambient")
                    {
                        color += ambient*lerp(albedo, spec_color, metalness);
                        continue;
                    } 
                    if(Light.type == L"direct")
                    {
                        vec3 L = -normalize(vec3(Light.direction, 3));// * Light.intensity;

                        traverse_result tv2_res =  traverse(hit_pos, L, object_world_from_mdl, 
                            object_mdl_from_world, Renderables, hit);
                        if(tv2_res.hit < 0) // we WANT this ray to miss!
                        {
                            L *= LLum;
                            //color += phong(view, L, hit_normal, albedo, spec_color, glossiness, metalness);
                            color += blinn_phong(view, L, hit_normal, albedo, spec_color, glossiness, metalness);
                        }
                    }
                    if(Light.type == L"point")
                    {
                        vec3 Lx = vec3(Light.position,3);
                        vec3 dL = Lx - hit_pos;
                        float dl2 = dot(dL,dL);
                        vec3 L = normalize(dL);// * Light.intensity;

                        traverse_result tv2_res =  traverse(hit_pos, L, object_world_from_mdl, 
                            object_mdl_from_world, Renderables, hit);
                        //if(tv2_res.hit < 0 || tv2_res.dist*tv2_res.dist <= dl2)
                        if(dl2 <= tv2_res.dist*tv2_res.dist)
                        {
                            L *= LLum;///dl2;
                            //vec3 color_new = phong(view, L, hit_normal, albedo, spec_color, glossiness, metalness);
                            vec3 color_new = blinn_phong(view, L, hit_normal, albedo, spec_color, glossiness, metalness);
                            color += color_new*100.0f/dl2;
                        }
                    }

                }

                
            } while(false); //else {
            /*
            if (hit < 0) // Lets us break from the above if
            {
                // background color (e.g., sky gradient)
                //color = vec3{0.2f, 0.3f, 0.6f}; // tweak as desired
                color = ambient;
            }*/
            //float cblnd = exp(-0.1*t_min);
            //color = color*cblnd + (1.0-cblnd)*ambient;
            //float cblnd = 0.5;
            //if(0.9 < sin(10.0f*t_min))
            //    color = color*cblnd + (1.0-cblnd)*ambient;

            //color *= 10.0f;
            color = max(color, vec3{0});
            color = saturationClip(color);
            color = pow(color, vec3{1.0/2.2});

            backbuffer[(cam.w*iv + iu)*3 + 0] += color[0];
            backbuffer[(cam.w*iv + iu)*3 + 1] += color[1];
            backbuffer[(cam.w*iv + iu)*3 + 2] += color[2];
        }
    }

    }

    float flim = float(spp_lim);
    for(auto &bb : backbuffer) bb /= flim;

    sdl_display_bbuffer(backbuffer, cam.w, cam.h);
    

    return 0;
}




