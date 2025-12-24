
// Joseph Kessler
// 2025 December 22

#include <array>
#include <cstddef>
#include <cmath>

#include "einsum_variadic_ct.hpp"

// Adapted from a template ChatGPT 5.2 wrote on 2025 December 22.
// Quotes from it are suppied in "".
template<typename T,std::size_t N>
struct vec
{
    using value_type = T; // Avoids retemplate hell
    std::array<T,N> array;

    vec() = default;
    //     "Explicit prevents type conversions, so putting it here is saying:
    // "This constructor is not a conversion." You must call it explicitly.
    // Since vec uses an array not std::vector, this constructor is redundant."
    // explicit vec(std::size_t n, const T& v = T{}) : array(n, v) {} 

    //     "vec() = default; explicitly asks for the compiler-generated default 
    // constructor. You did not declare copy/move constructors/assignments, so the 
    // compiler will generate them automatically because all members (std::array)
    // are copyable/movable."

    //     "The variadic constructor you added is not part of the “rule of 5.”
    // It’s an extra constructor to support vec{1,2,3} with compile-time checking."
    template<class... U>
    requires (sizeof...(U) == N) && (std::conjunction_v<std::is_convertible<U, T>...>)
    constexpr vec(U&&... xs) : array{ { static_cast<T>(std::forward<U>(xs))... } } {}

    // "If an exception propagates out, the program terminates. Treat it as a promice everything below will never crash."
    //vec<float,3> v(1.0f);   // fill
    //vec<float,3> v{1.0f};   // would call the variadic ctor with 1 arg (but fails because N=3)
    explicit vec(const T& fill) { array.fill(fill); }
    vec(const std::vector<T>& v) noexcept : vec(v.data(), v.size()) {} 
    vec(const T* arr, std::size_t arr_len)
    {
        if(arr == nullptr) return;
        size_t lim = std::min(N, arr_len);
        for(std::size_t n=0; n<lim; n++)
            array[n] = arr[n];
    }

    // Imported from array. May not be necissary!
    static constexpr std::size_t size() noexcept { return N; } // or constexpr member fn
    T* data() noexcept { return array.data(); }
    const T* data() const noexcept { return array.data(); }

    T& operator[](std::size_t i) noexcept { return array[i]; }
    const T& operator[](std::size_t i) const noexcept { return array[i]; }

    auto begin() noexcept { return array.begin(); }
    auto end() noexcept { return array.end(); }
    auto begin() const noexcept { return array.begin(); }
    auto end() const noexcept { return array.end(); }
private:

    template<class Op>
    static constexpr vec broadcast(const vec& a, const vec& b, Op op) {
        vec result{};
        for (std::size_t n=0; n<N; ++n) result[n] = op(a[n], b[n]);
        return result;
    }

    template<class Op>
    static constexpr vec broadcast(const vec& a, const T& b, Op op) {
        vec result{};
        for (std::size_t n=0; n<N; ++n) result[n] = op(a[n], b);
        return result;
    }

    template<class Op>
    static constexpr vec broadcast(const T& a, const vec& b, Op op) {
        vec result{};
        for (std::size_t n=0; n<N; ++n) result[n] = op(a, b[n]);
        return result;
    }

public:
    friend constexpr vec operator-(const vec& a) {
        vec r{};
        for (std::size_t i = 0; i < N; ++i) r[i] = -a[i];
        return r;
    }

    friend vec operator+(const vec& a, const vec& b) { return broadcast(a, b, [](T x, T y){ return x + y; }); }
    friend vec operator+(const vec& a, const   T& b) { return broadcast(a, b, [](T x, T y){ return x + y; }); }
    friend vec operator+(const   T& a, const vec& b) { return broadcast(a, b, [](T x, T y){ return x + y; }); }
    vec&       operator+=(const vec& b) { for (std::size_t i=0;i<N;++i)                      array[i]+=b[i]; return *this; }
    vec&       operator+=(const   T& s) { for (std::size_t i=0;i<N;++i)                      array[i]+=s;    return *this; }

    friend vec operator-(const vec& a, const vec& b) { return broadcast(a, b, [](T x, T y){ return x - y; }); }
    friend vec operator-(const vec& a, const   T& b) { return broadcast(a, b, [](T x, T y){ return x - y; }); }
    friend vec operator-(const   T& a, const vec& b) { return broadcast(a, b, [](T x, T y){ return x - y; }); }
    vec&       operator-=(const vec& b) { for (std::size_t i=0;i<N;++i)                      array[i]-=b[i]; return *this; }
    vec&       operator-=(const   T& s) { for (std::size_t i=0;i<N;++i)                      array[i]-=s;    return *this; }

    friend vec operator*(const vec& a, const vec& b) { return broadcast(a, b, [](T x, T y){ return x * y; }); }
    friend vec operator*(const vec& a, const   T& b) { return broadcast(a, b, [](T x, T y){ return x * y; }); }
    friend vec operator*(const   T& a, const vec& b) { return broadcast(a, b, [](T x, T y){ return x * y; }); }
    vec&       operator*=(const vec& b) { for (std::size_t i=0;i<N;++i)                      array[i]*=b[i]; return *this; }
    vec&       operator*=(const   T& s) { for (std::size_t i=0;i<N;++i)                      array[i]*=s;    return *this; }
    
    friend vec operator/(const vec& a, const vec& b) { return broadcast(a, b, [](T x, T y){ return x / y; }); }
    friend vec operator/(const vec& a, const   T& b) { return broadcast(a, b, [](T x, T y){ return x / y; }); }
    friend vec operator/(const   T& a, const vec& b) { return broadcast(a, b, [](T x, T y){ return x / y; }); }
    vec&       operator/=(const vec& b) { for (std::size_t i=0;i<N;++i)                      array[i]/=b[i]; return *this; }
    vec&       operator/=(const   T& s) { for (std::size_t i=0;i<N;++i)                      array[i]/=s;    return *this; }
};

template<class T, std::size_t N, int... L, std::size_t... S>
inline auto as_tensor(vec<T,N>& x, labels_t<L...>, shape_t<S...>)
{
    static_assert((S * ... * 1) == N);
    return make_tensor<T, labels_t<L...>, shape_t<S...>>(x.data());
}

template<class T, std::size_t N, int... L, std::size_t... S>
inline auto as_tensor(const vec<T,N>& x, labels_t<L...>, shape_t<S...>)
{
    static_assert((S * ... * 1) == N);
    return make_tensor<const T, labels_t<L...>, shape_t<S...>>(x.data()); // if supported
}

template<class T, std::size_t N, class F>
constexpr vec<T,N> map_unary(const vec<T,N>& a, F f) {
    vec<T,N> r{};
    for (std::size_t i=0;i<N;++i) r[i] = f(a[i]);
    return r;
}

template<class T, std::size_t N, class F>
constexpr vec<T,N> map_binary(const vec<T,N>& a, const vec<T,N>& b, F f) {
    vec<T,N> r{};
    for (std::size_t i=0;i<N;++i) r[i] = f(a[i], b[i]);
    return r;
}

template<class T, std::size_t N, class F>
constexpr vec<T,N> map_binary(const T& a, const vec<T,N>& b, F f) {
    vec<T,N> r{};
    for (std::size_t i=0;i<N;++i) r[i] = f(a, b[i]);
    return r;
}

template<class T, std::size_t N, class F>
constexpr vec<T,N> map_binary(const vec<T,N>& a, const T& b, F f) {
    vec<T,N> r{};
    for (std::size_t i=0;i<N;++i) r[i] = f(a[i], b);
    return r;
}

template<class T, std::size_t N, class F>
constexpr T reduce(const vec<T,N>& a, T init, F f) {
    for (std::size_t i=0;i<N;++i) init = f(init, a[i]);
    return init;
}

#define DEFINE_VEC_MAP(NAME) \
template<class T, std::size_t N> \
vec<T,N> NAME(const vec<T,N>& a) { using std::NAME; return map_unary(a, [](T x){ return NAME(x); });}

#define DEFINE_VEC_MAP2(NAME) \
template<class T, std::size_t N> \
vec<T,N> NAME(const vec<T,N>& a, const vec<T,N>& b) { using std::NAME; return map_binary(a, b, [](T x, T y){ return NAME(x, y); });} \
template<class T, std::size_t N> \
vec<T,N> NAME(const        T& a, const vec<T,N>& b) { using std::NAME; return map_binary(a, b, [](T x, T y){ return NAME(x, y); });} \
template<class T, std::size_t N> \
vec<T,N> NAME(const vec<T,N>& a, const        T& b) { using std::NAME; return map_binary(a, b, [](T x, T y){ return NAME(x, y); });}

#define DEFINE_VEC_REDUCE(NAME) \
template<class T, std::size_t N> \
constexpr T NAME(const vec<T,N>& a) {static_assert(N > 0); using std::NAME; return reduce(a, a[0], [](T acc, T x){ return NAME(acc, x); });}

DEFINE_VEC_REDUCE(min);
DEFINE_VEC_REDUCE(max);

DEFINE_VEC_MAP(abs);
DEFINE_VEC_MAP(sqrt);
DEFINE_VEC_MAP(exp);
DEFINE_VEC_MAP(log);
DEFINE_VEC_MAP(log2);
DEFINE_VEC_MAP(log10);

DEFINE_VEC_MAP(sin);
DEFINE_VEC_MAP(cos);
DEFINE_VEC_MAP(tan);
DEFINE_VEC_MAP(acos);
DEFINE_VEC_MAP(asin);
DEFINE_VEC_MAP(atan);

DEFINE_VEC_MAP(sinh);
DEFINE_VEC_MAP(cosh);
DEFINE_VEC_MAP(tanh);
DEFINE_VEC_MAP(asinh);
DEFINE_VEC_MAP(acosh);
DEFINE_VEC_MAP(atanh);

DEFINE_VEC_MAP2(pow);
DEFINE_VEC_MAP2(atan2);

// These are technically binary dots, but I dont think the compiler would be happy if I did that.
template<typename T,std::size_t N>
T sum(const vec<T,N>& arr)
{
    T res{};
    for (std::size_t n=0;n<N;++n)
        res += arr[n];
    return res;
}

template<typename T,std::size_t N>
T prod(const vec<T,N>& arr)
{
    T res{1};
    for (std::size_t n=0;n<N;++n)
        res *= arr[n];
    return res;
}

template<typename T,std::size_t N>
T dot(const vec<T,N>& a, const vec<T,N>& b) {
    return sum(a*b);
}

template<typename T>
constexpr vec<T,3> cross(const vec<T,3>& a, const vec<T,3>& b) {
    // Right-hand rule: a × b
    return vec<T,3>(
        a[1]*b[2] - a[2]*b[1],  // x
        a[2]*b[0] - a[0]*b[2],  // y
        a[0]*b[1] - a[1]*b[0]   // z
    );
}

template<typename T,std::size_t N>
T mag(const vec<T,N>& a) {
    using std::sqrt;
    return sqrt(dot(a,a));
}

template<typename T,std::size_t N>
vec<T,N> normalize(const vec<T,N>& a) {
    T m = mag(a);
    if(m == T{}) return a;
    return a / m;
}

template<typename T, std::size_t N>
T lse(const vec<T,N>& x)
{
    using std::log;
    T m = max(x);
    T Z = sum(exp(x - m));          // exp is lifted, -m broadcasts
    return m + log(Z);
}

template<typename T, std::size_t N>
vec<T,N> softmax(const vec<T,N>& x)
{
    T m = max(x);
    vec<T,N> y = exp(x - m);
    T Z = sum(y);
    return y / Z;
}

template<typename T, std::size_t N>
vec<T,N> log_softmax(const vec<T,N>& x)
{
    return x - lse(x);   // broadcasts scalar subtract
}


