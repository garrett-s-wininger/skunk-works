#ifndef ENDIAN_H
#define ENDIAN_H

#include <bit>
#include <cstdint>

#ifdef __cplusplus
extern "C++" {

namespace endian {

template <typename T>
concept MultiByteIntegral =
    std::same_as<T, uint8_t>
    || std::same_as<T, uint16_t>
    || std::same_as<T, uint32_t>;

template <MultiByteIntegral V>
auto big(V value) {
    if constexpr (std::same_as<V, uint8_t>) {
        return value;
    } else {
        V result = value;

        if constexpr (std::endian::native == std::endian::big) {
            return result;
        } else {
            return std::byteswap(result);
        }
    }
}

}

}
#endif

#endif // ENDIAN_H
