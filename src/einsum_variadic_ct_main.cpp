
#include "einsum_variadic_ct.hpp"
#include <iostream>
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

/*
// Xorshift stuff
__device__
uint32_t xorshift32(uint32_t *rng_seed)
{
    uint32_t x = *rng_seed;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *rng_seed = x;
}

__device__
float xorshiftflt(uint32_t *rng_seed)
{
    float res = 1.0f;
    uint32_t bits = *(uint32_t *)&res;
    bits = bits | (xorshift32(rng_seed) >> 9);
    return (*(float*)&bits) - 1.0f;
}
*/

// Choose type: define TEST_FLOAT to use float, otherwise double is used by default
#define TEST_FLOAT
#ifdef TEST_FLOAT
using testtype = float;
#define rng() xorshiftflt()
#else
using testtype = double;
#define rng() xorshiftdbl()
#endif

int main() {

    const size_t max_iters = 40000;

    std::vector<double> d10;
    std::vector<double> d21;
    std::vector<double> d32;
    std::vector<double> d43;
    std::vector<double> d40;
    std::vector<double> results;
    std::vector<double> resA;
    std::vector<double> resB;

    for(size_t iters=0; iters<max_iters; iters++)
    {
        constexpr std::size_t N = 40;

        testtype Gamma[N*N*N] = {}; // Γ^k_{ij}
        testtype v[N] = {};//rng(),rng(),rng(),rng()};//{1,2,3,4};
        testtype v2[N] = {};//rng(),rng(),rng(),rng()};//{1,2,3,4};
        testtype a[N] = {};
        for(std::size_t n=0; n<N; n++)
        {
            v[n] = rng();
            v2[n] = rng();
        }

        auto t0 = std::chrono::high_resolution_clock::now();

        // toy: Γ^k_{ij} = 1 if k==i==j else 0
        for (std::size_t k=0;k<N;++k)
            for (std::size_t i=0;i<N;++i)
                for (std::size_t j=0;j<N;++j)
                    Gamma[k*N*N + i*N + j] = rng()*2.0f - 1.0f;//(k==i && i==j) ? 1.0f : 0.0f;

        auto t1 = std::chrono::high_resolution_clock::now();

        // labels: k=0, i=1, j=2
        auto G  = make_tensor<testtype, labels_t<0,1,2>, shape_t<N,N,N>>(Gamma);
        auto vi = make_tensor<testtype, labels_t<1>,     shape_t<N>>(v);
        auto vj = make_tensor<testtype, labels_t<2>,     shape_t<N>>(v2);

        auto t2 = std::chrono::high_resolution_clock::now();

        // a^k = Γ^k_{ij} v^i v^j -> output label k = 0
        einsum_into<0>(a, G, vi, vj);

        auto t3 = std::chrono::high_resolution_clock::now();

        testtype b[N] = {};
        for (std::size_t k=0;k<N;++k)
            for (std::size_t i=0;i<N;++i)
                for (std::size_t j=0;j<N;++j)
                    b[k] += Gamma[k*N*N + i*N + j]*v[i]*v2[j];

        auto t4 = std::chrono::high_resolution_clock::now();

        typedef std::chrono::nanoseconds nanoseconds;
        d10.push_back(std::chrono::duration<double>(t1 - t0).count()); // fill Gamma
        d21.push_back(std::chrono::duration<double>(t2 - t1).count()); // make_tensor wrappers
        d32.push_back(std::chrono::duration<double>(t3 - t2).count()); // einsum_into
        d43.push_back(std::chrono::duration<double>(t4 - t3).count()); // naive triple-loop reference
        d40.push_back(std::chrono::duration<double>(t4 - t0).count()); // total

        testtype worst = 0.0;
        testtype worstA = 0.0;
        testtype worstB = 0.0;
        for(std::size_t n=0; n<N; n++)
        {
            worst = std::max(worst, std::abs((a[n]-b[n])/b[n]));
            worstA = std::max(worstA, std::abs(a[n]));
            worstB = std::max(worstB, std::abs(b[n]));
        }
        results.push_back(worst);
        resA.push_back(worstA);
        resB.push_back(worstB);
    }

// Compute mean and variance
    auto compute_stats = [](const std::vector<double>& v) {
        double mean = 0.0;
        const std::size_t n = v.size();
        if (n == 0) return std::tuple<double,double,double>(0.0, 0.0, 0.0);
        for (double x : v) mean += x;
        mean /= double(n);
        double sumsq = 0.0;
        for (double x : v) {
        double d = x - mean;
        sumsq += d * d;
        }
        // sample variance (n>1) otherwise zero
        double variance = (n > 1) ? (sumsq / double(n - 1)) : 0.0;
        double stddev = std::sqrt(variance);
        return std::tuple<double,double,double>(mean, variance, stddev);
    };

    auto [m10, var10, sd10] = compute_stats(d10);
    auto [m21, var21, sd21] = compute_stats(d21);
    auto [m32, var32, sd32] = compute_stats(d32);
    auto [m43, var43, sd43] = compute_stats(d43);
    auto [m40, var40, sd40] = compute_stats(d40);
    auto [mRS, varRS, sdRS] = compute_stats(results);
    auto [mAS, varAS, sdAS] = compute_stats(resA);
    auto [mBS, varBS, sdBS] = compute_stats(resB);

    // convert seconds -> nanoseconds for printing (the stored durations are seconds)
    constexpr double to_ns = 1e9;

    std::cout << std::endl;
    std::cout << (m10 * to_ns) << " ns +- " << (sd10 * to_ns) << " ns"
              << " (variance = " << (var10 * to_ns * to_ns) << " ns^2)"
              << ": initialize tensor data (fill Gamma)"
              << "\n";
    std::cout << (m21 * to_ns) << " ns +- " << (sd21 * to_ns) << " ns"
              << " (variance = " << (var21 * to_ns * to_ns) << " ns^2)"
              << ": construct tensor views / label binding (make_tensor)"
              << "\n";
    std::cout << (m32 * to_ns) << " ns +- " << (sd32 * to_ns) << " ns"
              << " (variance = " << (var32 * to_ns * to_ns) << " ns^2)"
              << ": einsum_into (optimized contraction)"
              << "\n";
    std::cout << (m43 * to_ns) << " ns +- " << (sd43 * to_ns) << " ns"
              << " (variance = " << (var43 * to_ns * to_ns) << " ns^2)"
              << ": naive triple-loop reference computation"
              << "\n";
    std::cout << (m40 * to_ns) << " ns +- " << (sd40 * to_ns) << " ns"
              << " (variance = " << (var40 * to_ns * to_ns) << " ns^2)"
              << ": total time (all steps)"
              << "\n";
    std::cout << std::endl;
    std::cout << mRS << " +-" << sdRS << ": Worst absolute error" << std::endl;
    std::cout << mAS << " +-" << sdAS << ": Biggest A - To get the compiler to behave" << std::endl;
    std::cout << mBS << " +-" << sdBS << ": Biggest B - To get the compiler to behave" << std::endl;


}

/*
int main() {
    constexpr std::size_t N = 40;

    testtype Gamma[N*N*N] = {}; // Γ^k_{ij}
    testtype v[N] = {};//rng(),rng(),rng(),rng()};//{1,2,3,4};
    testtype v2[N] = {};//rng(),rng(),rng(),rng()};//{1,2,3,4};
    testtype a[N] = {};
    for(std::size_t n=0; n<N; n++)
    {
        v[n] = rng();
        v2[n] = rng();
    }

    auto t0 = std::chrono::high_resolution_clock::now();

    // toy: Γ^k_{ij} = 1 if k==i==j else 0
    for (std::size_t k=0;k<N;++k)
        for (std::size_t i=0;i<N;++i)
            for (std::size_t j=0;j<N;++j)
                Gamma[k*N*N + i*N + j] = rng()*2.0f - 1.0f;//(k==i && i==j) ? 1.0f : 0.0f;

    auto t1 = std::chrono::high_resolution_clock::now();

    // labels: k=0, i=1, j=2
    auto G  = make_tensor<testtype, labels_t<0,1,2>, shape_t<N,N,N>>(Gamma);
    auto vi = make_tensor<testtype, labels_t<1>,     shape_t<N>>(v);
    auto vj = make_tensor<testtype, labels_t<2>,     shape_t<N>>(v2);

    auto t2 = std::chrono::high_resolution_clock::now();

    // a^k = Γ^k_{ij} v^i v^j -> output label k = 0
    einsum_into<0>(a, G, vi, vj);

    auto t3 = std::chrono::high_resolution_clock::now();

    testtype b[N] = {};
    for (std::size_t k=0;k<N;++k)
        for (std::size_t i=0;i<N;++i)
            for (std::size_t j=0;j<N;++j)
                b[k] += Gamma[k*N*N + i*N + j]*v[i]*v2[j];

    auto t4 = std::chrono::high_resolution_clock::now();

    typedef std::chrono::nanoseconds nanoseconds;
    auto d10 = std::chrono::duration_cast<nanoseconds>(t1 - t0).count(); // fill Gamma
    auto d21 = std::chrono::duration_cast<nanoseconds>(t2 - t1).count(); // make_tensor wrappers
    auto d32 = std::chrono::duration_cast<nanoseconds>(t3 - t2).count(); // einsum_into
    auto d43 = std::chrono::duration_cast<nanoseconds>(t4 - t3).count(); // naive triple-loop reference
    auto d40 = std::chrono::duration_cast<nanoseconds>(t4 - t0).count(); // total
    std::cout << std::endl;
    std::cout << "initialize tensor data (fill Gamma): " << d10 << " ns\n";
    std::cout << "construct tensor views / label binding (make_tensor): " << d21 << " ns\n";
    std::cout << "einsum_into (optimized contraction): " << d32 << " ns\n";
    std::cout << "naive triple-loop reference computation: " << d43 << " ns\n";
    std::cout << "total time (all steps): " << d40 << " ns\n";
    std::cout << std::endl;
    
    for (std::size_t k=0;k<N;++k)
        std::cout << "a[" << k << "]=" << a[k] << "\n";

    std::cout << std::endl;
    for (std::size_t k=0;k<N;++k)
        std::cout << "b[" << k << "]=" << b[k] << "\n";

    std::cout << std::endl;
    for (std::size_t k=0;k<N;++k)
        std::cout << "(a - b)[" << k << "]=" << a[k] - b[k] << "\n";

    std::cout << std::endl;
    for (std::size_t k=0;k<N;++k)
        std::cout << "|1 - a/b|[" << k << "]=" << std::abs((a[k] - b[k])/b[k]) << "\n";

    std::cout << std::endl;
    for (std::size_t k=0;k<N;++k)
        std::cout << "|1 - a/b|[" << k << "]=" << std::log(std::abs((a[k] - b[k])/b[k]))/std::log(2.0) << "\n";
}
*/