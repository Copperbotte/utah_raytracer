#pragma once
#include <array>
#include <cstddef>
#include <type_traits>
#include <tuple>
#include <utility>

// carriers
template<int... Ls> struct labels_t {};
template<std::size_t... Ds> struct shape_t {};

// tensor
template<class T, class Labels, class Shape>
struct tensor;

template<class T, int... Labels, std::size_t... Shape>
struct tensor<T, labels_t<Labels...>, shape_t<Shape...>>
{
    static constexpr std::size_t rank = sizeof...(Labels);
    static_assert(sizeof...(Shape) == rank, "shape count must match rank");
    // using value_type = T;
    // T* data;
    // Another ChatGPT 5.2 instance suggested replacing that with the following to allow for consts:
    using scalar_type = std::remove_const_t<T>;
    using value_type  = scalar_type;
    using pointer = std::conditional_t<
        std::is_const_v<T>,
        const scalar_type*,
        scalar_type*
    >;
    pointer data;

    static consteval std::array<int, rank> labels() { return {Labels...}; }
    static consteval std::array<std::size_t, rank> shape() { return {Shape...}; }

    static consteval std::array<std::size_t, rank> stride()
    {
        std::array<std::size_t, rank> st{};
        std::size_t acc = 1;
        auto sh = shape();
        for (std::size_t d = rank; d-- > 0; ) { st[d] = acc; acc *= sh[d]; }
        return st;
    }
};

template<class T, class Labels, class Shape>
constexpr tensor<T, Labels, Shape> make_tensor(T* ptr) { return {ptr}; }
template<class Labels, class Shape, class T>
constexpr tensor<T, Labels, Shape> make_tensor(T* ptr) { return {ptr}; }

template<class T, class Labels, class Shape>
constexpr tensor<const T, Labels, Shape> make_tensor(const T* ptr) { return {ptr}; }
template<class Labels, class Shape, class T>
constexpr tensor<const T, Labels, Shape> make_tensor(const T* ptr) { return {ptr}; }


// helpers (no abbreviated auto)
template<class Arr>
constexpr int find_axis(const Arr& lbls, int x)
{
    for (std::size_t i = 0; i < lbls.size(); ++i) if (lbls[i] == x) return (int)i;
    return -1;
}

template<class Arr>
constexpr bool all_unique_labels(const Arr& lbls)
{
    for (std::size_t i = 0; i < lbls.size(); ++i)
        for (std::size_t j = i + 1; j < lbls.size(); ++j)
            if (lbls[i] == lbls[j]) return false;
    return true;
}

template<std::size_t Rank>
consteval std::array<std::size_t, Rank> make_row_major_stride(const std::array<std::size_t, Rank>& sh)
{
    std::array<std::size_t, Rank> st{};
    std::size_t acc = 1;
    for (std::size_t d = Rank; d-- > 0; ) { st[d] = acc; acc *= sh[d]; }
    return st;
}

// out traits (fixes "partial specialization of function template" issue)
template<class OutLabels> struct out_traits;

template<int... OL>
struct out_traits<labels_t<OL...>>
{
    static consteval int max_label()
    {
        int m = -1;
        int tmp[] = {OL...};
        for (int v : tmp) if (v > m) m = v;
        return m;
    }
    static consteval std::size_t count_label(int lab)
    {
        std::size_t c = 0;
        int tmp[] = {OL...};
        for (int v : tmp) if (v == lab) ++c;
        return c;
    }
    static consteval std::array<int, sizeof...(OL)> array() { return {OL...}; }
    static constexpr std::size_t rank = sizeof...(OL);
};

template<class Ten>
consteval int max_label_in_tensor()
{
    auto lbl = Ten::labels();
    int m = -1;
    for (int v : lbl) if (v > m) m = v;
    return m;
}

template<class OutLabels, class... Ts>
consteval int max_label_all()
{
    int m = out_traits<OutLabels>::max_label();
    ((m = (max_label_in_tensor<Ts>() > m ? max_label_in_tensor<Ts>() : m)), ...);
    return m;
}

template<class Ten>
consteval std::size_t count_label_in_tensor(int lab)
{
    auto lbl = Ten::labels();
    std::size_t c = 0;
    for (int v : lbl) if (v == lab) ++c;
    return c;
}

template<class OutLabels, class... Ts>
consteval std::size_t count_label_everywhere(int lab)
{
    std::size_t c = out_traits<OutLabels>::count_label(lab);
    ((c += count_label_in_tensor<Ts>(lab)), ...);
    return c;
}

template<class Ten>
consteval bool tensor_contains_label(int lab)
{
    return count_label_in_tensor<Ten>(lab) != 0;
}

template<class Ten>
consteval std::size_t extent_for_label_in_tensor(int lab)
{
    auto lbl = Ten::labels();
    auto sh  = Ten::shape();
    int ax = find_axis(lbl, lab);
    return (ax >= 0) ? sh[(std::size_t)ax] : 0;
}

template<class... Ts>
consteval std::size_t tensors_with_label(int lab)
{
    return (std::size_t(0) + ... + (tensor_contains_label<Ts>(lab) ? 1u : 0u));
}

template<class OutLabels, class... Ts>
consteval std::size_t contracted_count()
{
    constexpr int M = max_label_all<OutLabels, Ts...>();
    std::size_t c = 0;
    for (int lab = 0; lab <= M; ++lab) {
        const std::size_t cnt = count_label_everywhere<OutLabels, Ts...>(lab);
        if (cnt == 0) continue;
        if (out_traits<OutLabels>::count_label(lab) == 0) ++c;
    }
    return c;
}

template<std::size_t NC, class OutLabels, class... Ts>
consteval std::array<int, NC> build_contracted()
{
    constexpr int M = max_label_all<OutLabels, Ts...>();
    std::array<int, NC> c{};
    std::size_t k = 0;
    for (int lab = 0; lab <= M; ++lab) {
        const std::size_t cnt = count_label_everywhere<OutLabels, Ts...>(lab);
        if (cnt == 0) continue;
        if (out_traits<OutLabels>::count_label(lab) == 0) c[k++] = lab;
    }
    return c;
}

// einsum kernel
template<class OutLabels, class... Ts>
struct einsumN
{
    static_assert(sizeof...(Ts) >= 1);

    using T0 = std::tuple_element_t<0, std::tuple<Ts...>>;
    using T  = typename T0::value_type;

    static_assert((std::is_same_v<typename Ts::value_type, T> && ...),
                  "einsumN: all tensors must share value_type");

    static constexpr int maxLab = max_label_all<OutLabels, Ts...>();
    static constexpr std::size_t OR = out_traits<OutLabels>::rank;
    static constexpr std::size_t NC = contracted_count<OutLabels, Ts...>();
    static constexpr auto c_lbl = build_contracted<NC, OutLabels, Ts...>();

    static consteval void validate_no_trace()
    {
        static_assert((all_unique_labels(Ts::labels()) && ...),
                      "einsumN: repeated labels within a tensor (trace) not allowed");
    }

    template<int Lab>
    static consteval void validate_one_label()
    {
        constexpr std::size_t cnt = count_label_everywhere<OutLabels, Ts...>(Lab);
        if constexpr (cnt == 0) return;

        static_assert(cnt == 2, "einsumN: label must appear exactly twice (including output)");

        constexpr bool inOut = (out_traits<OutLabels>::count_label(Lab) != 0);
        constexpr std::size_t tw = tensors_with_label<Ts...>(Lab);

        if constexpr (inOut) static_assert(tw == 1, "einsumN: output label must appear in exactly one input");
        else                 static_assert(tw == 2, "einsumN: contracted label must appear in exactly two inputs");

        /*
        constexpr std::size_t e0 = []() consteval {
            std::size_t e = 0;
            auto pick = [&](std::size_t cand) consteval { if (cand != 0) e = cand; };
            (pick(extent_for_label_in_tensor<Ts>(Lab)), ...);
            return e;
        }();

        auto check = [&](std::size_t cand) consteval {
            if (cand != 0) static_assert(cand == e0, "einsumN: extent mismatch for label");
        };
        (check(extent_for_label_in_tensor<Ts>(Lab)), ...);
        */

        constexpr std::size_t e0 = []() consteval {
            std::size_t e = 0;
            // choose the first non-zero extent among tensors that contain Lab
            ((extent_for_label_in_tensor<Ts>(Lab) != 0 && e == 0
                ? (e = extent_for_label_in_tensor<Ts>(Lab), true)
                : false), ...);
            return e;
        }();

        // Now assert all non-zero extents equal e0, without lambda parameters:
        static_assert(
            (((extent_for_label_in_tensor<Ts>(Lab) == 0) ||
            (extent_for_label_in_tensor<Ts>(Lab) == e0)) && ...),
            "einsumN: extent mismatch for label"
        );


    }

    static consteval void validate()
    {
        validate_no_trace();
        []<int... Is>(std::integer_sequence<int, Is...>) consteval {
            (validate_one_label<Is>(), ...);
        }(std::make_integer_sequence<int, maxLab + 1>{});
    }

    static consteval std::array<std::size_t, OR> out_shape()
    {
        constexpr auto ol = out_traits<OutLabels>::array();
        std::array<std::size_t, OR> sh{};
        for (std::size_t d = 0; d < OR; ++d) {
            int lab = ol[d];
            std::size_t e = 0;
            auto pick = [&](std::size_t cand) consteval { if (cand != 0) e = cand; };
            (pick(extent_for_label_in_tensor<Ts>(lab)), ...);
            sh[d] = e;
        }
        return sh;
    }

    template<class Ten>
    static inline std::size_t offset_for(const Ten& ten, const std::array<std::size_t, maxLab + 1>& val)
    {
        const auto lbl = Ten::labels();
        const auto st  = Ten::stride();
        std::size_t off = 0;
        for (std::size_t a = 0; a < Ten::rank; ++a)
            off += val[(std::size_t)lbl[a]] * st[a];
        return off;
    }

    static inline T product_all(const std::array<std::size_t, maxLab + 1>& val, const Ts&... tens)
    {
        T eprod = T{1};
        ((eprod *= tens.data[offset_for(tens, val)]), ...);
        return eprod;
    }

    template<std::size_t CI, int Lab>
    static inline T contract_sum_lab(std::array<std::size_t, maxLab + 1>& val, const Ts&... tens)
    {
        // extent for Lab (compile-time, because Lab is a template parameter)
        constexpr std::size_t extent = []() consteval {
            std::size_t e = 0;
            ((extent_for_label_in_tensor<Ts>(Lab) != 0 && e == 0
                ? (e = extent_for_label_in_tensor<Ts>(Lab), true)
                : false), ...);
            return e;
        }();

        T esum{};
        for (std::size_t t = 0; t < extent; ++t) {
            val[(std::size_t)Lab] = t;

            if constexpr (CI + 1 == NC) {
                esum += product_all(val, tens...);
            } else {
                // Next label is also compile-time
                constexpr int NextLab = c_lbl[CI + 1];
                esum += contract_sum_lab<CI + 1, NextLab>(val, tens...);
            }
        }
        return esum;
    }

    static inline T contract_sum(std::array<std::size_t, maxLab + 1>& val, const Ts&... tens)
    {
        if constexpr (NC == 0) {
            return product_all(val, tens...);
        } else {
            constexpr int FirstLab = c_lbl[0];
            return contract_sum_lab<0, FirstLab>(val, tens...);
        }
    }


    template<std::size_t DI>
    static inline void loop_out(T* out,
                                std::array<std::size_t, maxLab + 1>& val,
                                std::array<std::size_t, OR>& idx,
                                const Ts&... tens)
    {
        constexpr auto osh = out_shape();
        constexpr auto ost = make_row_major_stride(osh);
        constexpr auto ol  = out_traits<OutLabels>::array();

        if constexpr (DI == OR) {
            std::size_t off = 0;
            for (std::size_t d = 0; d < OR; ++d) off += idx[d] * ost[d];
            out[off] = contract_sum(val, tens...);
        } else {
            const int lab = ol[DI];
            for (std::size_t v = 0; v < osh[DI]; ++v) {
                idx[DI] = v;
                val[(std::size_t)lab] = v;
                loop_out<DI + 1>(out, val, idx, tens...);
            }
        }
    }

    static inline void into(T* __restrict__ out, const Ts&... tens)
    {
        validate();
        std::array<std::size_t, OR> idx{};
        std::array<std::size_t, maxLab + 1> val{};
        loop_out<0>(out, val, idx, tens...);
    }
};

template<int... OutLabels, class... Ts>
inline void einsum_into(typename std::tuple_element_t<0, std::tuple<Ts...>>::value_type* out,
                        const Ts&... tensors)
{
    einsumN<labels_t<OutLabels...>, Ts...>::into(out, tensors...);
}
