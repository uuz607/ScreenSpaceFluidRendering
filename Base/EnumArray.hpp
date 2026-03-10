#pragma once

#include <array>
#include <cassert>
#include <type_traits>

namespace utils
{
    template<typename T, typename Enum>
    class EnumArray
    {
        static_assert(std::is_enum_v<Enum>, "EnumArray requires an enum type");

    public:
        static constexpr size_t size() noexcept
        {
            return Count;
        }

        // ----------------------------
        // Element access
        // ----------------------------

        T& operator[](Enum id) noexcept
        {
            return elements_[to_index(id)];
        }

        const T& operator[](Enum id) const noexcept
        {
            return elements_[to_index(id)];
        }

        T& at(Enum id)
        {
            const auto idx = to_index(id);
            assert(idx < Count);
            return elements_[idx];
        }

        const T& at(Enum id) const
        {
            const auto idx = to_index(id);
            assert(idx < Count);
            return elements_[idx];
        }

        // ----------------------------
        // Iterators
        // ----------------------------

        auto begin() noexcept { return elements_.begin(); }
        auto end()   noexcept { return elements_.end(); }

        auto begin() const noexcept { return elements_.begin(); }
        auto end()   const noexcept { return elements_.end(); }

        auto cbegin() const noexcept { return elements_.cbegin(); }
        auto cend()   const noexcept { return elements_.cend(); }

        // ----------------------------
        // Raw access
        // ----------------------------

        T* data() noexcept { return elements_.data(); }
        const T* data() const noexcept { return elements_.data(); }

    private:
        static constexpr size_t to_index(Enum e) noexcept
        {
            return static_cast<size_t>(e);
        }

        static constexpr size_t Count = static_cast<size_t>(Enum::Count);

        std::array<T, Count> elements_{};
    };

    template<typename Enum, typename Func>
    void forEachEnum(Func&& f)
    {
        static_assert(std::is_enum_v<Enum>, "for_each_enum requires enum type");

        constexpr size_t Count = static_cast<size_t>(Enum::Count);

        for (size_t i = 0; i < Count; ++i)
        {
            f(static_cast<Enum>(i));
        }
    }
}