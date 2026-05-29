#pragma once 

#include <algorithm> 
#include <cassert> 
#include <memory> 
#include <new> 
#include <type_traits> 
#include <utility> 

template <typename T> 
class RawMemory { 
public: 
    RawMemory() = default; 

    explicit RawMemory(size_t capacity) 
        : buffer_(capacity ? Allocate(capacity) : nullptr) 
        , capacity_(capacity) { 
    } 

    RawMemory(const RawMemory&) = delete; 
    RawMemory& operator=(const RawMemory&) = delete; 

    RawMemory(RawMemory&& other) noexcept 
        : buffer_(other.buffer_) 
        , capacity_(other.capacity_) { 
        other.buffer_ = nullptr; 
        other.capacity_ = 0; 
    } 

    RawMemory& operator=(RawMemory&& other) noexcept { 
        if (this != &other) { 
            Deallocate(buffer_); 

            buffer_ = other.buffer_; 
            capacity_ = other.capacity_; 

            other.buffer_ = nullptr; 
            other.capacity_ = 0; 
        } 
        return *this; 
    } 

    ~RawMemory() { 
        Deallocate(buffer_); 
    } 

    T* operator+(size_t offset) noexcept { 
        return buffer_ + offset; 
    } 

    const T* operator+(size_t offset) const noexcept { 
        return buffer_ + offset; 
    } 

    T& operator[](size_t index) noexcept { 
        return buffer_[index]; 
    } 

    const T& operator[](size_t index) const noexcept { 
        return buffer_[index]; 
    } 

    T* GetAddress() noexcept { 
        return buffer_; 
    } 

    const T* GetAddress() const noexcept { 
        return buffer_; 
    } 

    size_t Capacity() const noexcept { 
        return capacity_; 
    } 

    void Swap(RawMemory& other) noexcept { 
        std::swap(buffer_, other.buffer_); 
        std::swap(capacity_, other.capacity_); 
    } 

private: 
    static T* Allocate(size_t n) { 
        return static_cast<T*>(operator new(n * sizeof(T))); 
    } 

    static void Deallocate(T* buf) noexcept { 
        operator delete(buf); 
    } 

private: 
    T* buffer_ = nullptr; 
    size_t capacity_ = 0; 
}; 

template <typename T> 
class Vector { 
public: 
    using iterator = T*; 
    using const_iterator = const T*; 

public: 
    Vector() = default; 

    explicit Vector(size_t size) 
        : data_(size) 
        , size_(size) { 
        std::uninitialized_value_construct_n(data_.GetAddress(), size); 
    } 

    Vector(const Vector& other) 
        : data_(other.size_) 
        , size_(other.size_) { 
        std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress()); 
    } 

    Vector(Vector&& other) noexcept 
        : data_(std::move(other.data_)) 
        , size_(other.size_) { 
        other.size_ = 0; 
    } 

    ~Vector() { 
        std::destroy_n(data_.GetAddress(), size_); 
    } 

    Vector& operator=(const Vector& other) { 
        if (this != &other) { 
            if (other.size_ > data_.Capacity()) { 
                Vector tmp(other); 
                Swap(tmp); 
            } else { 
                size_t copy_count = std::min(size_, other.size_); 
                std::copy_n(other.data_.GetAddress(), copy_count, data_.GetAddress()); 

                if (size_ > other.size_) { 
                    std::destroy_n(data_.GetAddress() + other.size_, size_ - other.size_); 
                } else if (other.size_ > size_) { 
                    std::uninitialized_copy_n( 
                        other.data_.GetAddress() + size_, 
                        other.size_ - size_, 
                        data_.GetAddress() + size_); 
                } 

                size_ = other.size_; 
            } 
        } 
        return *this; 
    } 

    Vector& operator=(Vector&& other) noexcept { 
        if (this != &other) { 
            Swap(other); 
        } 
        return *this; 
    } 

    size_t Size() const noexcept { 
        return size_; 
    } 

    size_t Capacity() const noexcept { 
        return data_.Capacity(); 
    } 

    T& operator[](size_t index) noexcept { 
        return data_[index]; 
    } 

    const T& operator[](size_t index) const noexcept { 
        return data_[index]; 
    } 

    iterator begin() noexcept { 
        return data_.GetAddress(); 
    } 

    iterator end() noexcept { 
        return data_.GetAddress() + size_; 
    } 

    const_iterator begin() const noexcept { 
        return data_.GetAddress(); 
    } 

    const_iterator end() const noexcept { 
        return data_.GetAddress() + size_; 
    } 

    const_iterator cbegin() const noexcept { 
        return begin(); 
    } 

    const_iterator cend() const noexcept { 
        return end(); 
    } 

    void Reserve(size_t new_capacity) { 
        if (new_capacity <= data_.Capacity()) { 
            return; 
        } 

        RawMemory<T> new_data(new_capacity); 
        UninitializedMoveOrCopy(data_.GetAddress(), size_, new_data.GetAddress()); 
        std::destroy_n(data_.GetAddress(), size_); 
        data_.Swap(new_data); 
    } 

    void Resize(size_t new_size) { 
        if (new_size < size_) { 
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size); 
        } else if (new_size > size_) { 
            Reserve(new_size); 
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_); 
        } 

        size_ = new_size; 
    } 

    void PushBack(const T& value) { 
        EmplaceBack(value); 
    } 

    void PushBack(T&& value) { 
        EmplaceBack(std::move(value)); 
    } 

    template <typename... Args> 
    T& EmplaceBack(Args&&... args) { 
        return *Emplace(cend(), std::forward<Args>(args)...); 
    } 

    template <typename... Args> 
    iterator Emplace(const_iterator pos, Args&&... args) { 
        assert(pos >= begin() && pos <= end()); 
        size_t index = pos - cbegin(); 

        if (size_ == Capacity()) { 
            return EmplaceWithReallocation(index, std::forward<Args>(args)...); 
        } else { 
            return EmplaceWithoutReallocation(index, std::forward<Args>(args)...); 
        } 
    } 

    iterator Insert(const_iterator pos, const T& value) { 
        return Emplace(pos, value); 
    } 

    iterator Insert(const_iterator pos, T&& value) { 
        return Emplace(pos, std::move(value)); 
    } 

    iterator Erase(const_iterator pos) 
        noexcept(std::is_nothrow_move_assignable_v<T>) { 

        assert(pos >= begin() && pos < end()); 

        size_t index = pos - cbegin(); 

        std::move( 
            begin() + index + 1, 
            end(), 
            begin() + index); 

        PopBack(); 

        return begin() + index; 
    } 

    void PopBack() noexcept { 
        assert(size_ > 0); 
        data_[size_ - 1].~T(); 
        --size_; 
    } 

    void Swap(Vector& other) noexcept { 
        data_.Swap(other.data_); 
        std::swap(size_, other.size_); 
    } 

private: 
    static void UninitializedMoveOrCopy(T* from, size_t count, T* to) { 
        if constexpr (std::is_nothrow_move_constructible_v<T> || 
                      !std::is_copy_constructible_v<T>) { 
            std::uninitialized_move_n(from, count, to); 
        } else { 
            std::uninitialized_copy_n(from, count, to); 
        } 
    } 

    template <typename... Args> 
    iterator EmplaceWithReallocation(size_t index, Args&&... args) { 
        size_t new_capacity = size_ == 0 ? 1 : size_ * 2; 
        RawMemory<T> new_data(new_capacity); 

        T* new_elem = new (new_data + index) T(std::forward<Args>(args)...); 

        try { 
            UninitializedMoveOrCopy(data_.GetAddress(), index, new_data.GetAddress()); 
            UninitializedMoveOrCopy( 
                data_.GetAddress() + index, 
                size_ - index, 
                new_data.GetAddress() + index + 1); 
        } catch (...) { 
            new_elem->~T(); 
            throw; 
        } 

        std::destroy_n(data_.GetAddress(), size_); 
        data_.Swap(new_data); 
        ++size_; 

        return begin() + index; 
    } 

    template <typename... Args> 
    iterator EmplaceWithoutReallocation(size_t index, Args&&... args) { 
        if (index == size_) { 
            new (data_ + size_) T(std::forward<Args>(args)...); 
        } else { 
            T tmp(std::forward<Args>(args)...); 
            new (data_ + size_) T(std::move(data_[size_ - 1])); 
            std::move_backward(begin() + index, end() - 1, end()); 
            data_[index] = std::move(tmp); 
        } 

        ++size_; 
        return begin() + index; 
    } 

private: 
    RawMemory<T> data_; 
    size_t size_ = 0; 
};