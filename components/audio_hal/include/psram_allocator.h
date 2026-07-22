#ifndef AUDIO_HAL_PSRAM_ALLOCATOR_H_
#define AUDIO_HAL_PSRAM_ALLOCATOR_H_

#include <esp_heap_caps.h>

#include <cstddef>
#include <limits>
#include <new>
#include <vector>

template <typename T>
class PsramAllocator {
public:
    using value_type = T;

    PsramAllocator() noexcept = default;

    template <typename U>
    PsramAllocator(const PsramAllocator<U>&) noexcept {}

    [[nodiscard]] T* allocate(std::size_t count) {
        if (count > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
            throw std::bad_alloc();
        }

        void* memory = heap_caps_malloc(count * sizeof(T), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (memory == nullptr) {
            memory = heap_caps_malloc(count * sizeof(T), MALLOC_CAP_8BIT);
        }
        if (memory == nullptr) {
            throw std::bad_alloc();
        }
        return static_cast<T*>(memory);
    }

    void deallocate(T* pointer, std::size_t) noexcept {
        heap_caps_free(pointer);
    }

    template <typename U>
    bool operator==(const PsramAllocator<U>&) const noexcept {
        return true;
    }

    template <typename U>
    bool operator!=(const PsramAllocator<U>&) const noexcept {
        return false;
    }
};

template <typename T>
using PsramVector = std::vector<T, PsramAllocator<T>>;

#endif  // AUDIO_HAL_PSRAM_ALLOCATOR_H_
