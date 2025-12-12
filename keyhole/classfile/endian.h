#ifndef ENDIAN_H
#define ENDIAN_H

#include <bit>

namespace endian {

template <typename T>
concept MultiByteIntegral = std::same_as<T, uint16_t> || std::same_as<T, uint32_t>;

template <MultiByteIntegral V>
auto big(V value) {
    V result = value;

    if constexpr (std::endian::native == std::endian::big) {
        return result;
    } else {
        return std::byteswap(result);
    }
}

}

#endif // ENDIAN_H
