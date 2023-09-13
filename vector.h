
#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <algorithm>


template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;

    RawMemory(RawMemory&& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    RawMemory& operator=(RawMemory&& rhs) noexcept {
        std::swap(buffer_, rhs.buffer_);
        std::swap(capacity_, rhs.capacity_);
        return *this;
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {

public:

    using iterator = T*;
    using const_iterator = const T*;
    Vector() = default;

    explicit Vector(size_t size)
        : data_(size)
        , size_(size)
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_)
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), other.Size(), data_.GetAddress());
    }

    Vector(Vector&& other) noexcept
        : data_(std::move(other.data_))
        , size_(std::move(other.size_))
    {
        other.size_ = 0;
    }


    Vector& operator=(const Vector& rhs) {
        if (rhs.Size() > data_.Capacity()) {
            Vector rhs_copy(rhs);
            Swap(rhs_copy);
            return *this;
        }

        if (rhs.Size() < Size()) {
            for (size_t i = 0; i < rhs.Size(); ++i) {
                data_[i] = rhs[i];
            }
            std::destroy_n(data_.GetAddress() + rhs.Size(), Size() - rhs.Size());
        }
        else {
            for (size_t i = 0; i < Size(); ++i) {
                data_[i] = rhs[i];
            }
            std::uninitialized_copy_n(rhs.data_.GetAddress() + Size(), rhs.Size() - Size(), data_.GetAddress() + Size());
        }

        size_ = rhs.size_;
        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept {
        data_ = std::move(rhs.data_);
        size_ = rhs.size_;
        rhs.size_ = 0;
        return *this;
    }

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }




    void Swap(Vector& other) noexcept {

        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }


    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        RawMemory<T> new_data(new_capacity);
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        std::destroy_n(data_.GetAddress(), size_);

        data_.Swap(new_data);
    }

    void Resize(size_t new_size) {
        if (new_size < size_) {
            DestroyN(data_.GetAddress() + new_size, size_ - new_size);
        }
        else {
            this->Reserve(new_size);
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
        }
        size_ = new_size;

    }
    template<typename Type>
    void PushBack(Type&& value) {
        if (data_.Capacity() > size_)
        {
            new (data_.GetAddress() + Size()) T(std::forward<Type>(value));
        }
        else {
            RawMemory<T> new_data(Size() == 0 ? 1 : Size() * 2);
            new (new_data.GetAddress() + Size()) T(std::forward<Type>(value));
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        }

        size_++;

    }

    void PopBack() {

        std::destroy_n(data_.GetAddress() + size_ - 1, 1);
        size_--;
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }


    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        if (data_.Capacity() > size_)
        {
            new (data_.GetAddress() + Size()) T(std::forward<Args>(args)...);
        }
        else {
            RawMemory<T> new_data(Size() == 0 ? 1 : Size() * 2);
            new (new_data.GetAddress() + Size()) T(std::forward<Args>(args)...);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        }

        size_++;
        return *(data_.GetAddress() + size_ - 1);
    }

    template <typename... Args>
    iterator Emplace(const_iterator position, Args&&... args) {
        int pos = position - begin();

        if (data_.Capacity() > Size()) {
            try {
                if (position != end()) {
                    T tmp_obj(std::forward<Args>(args)...);
                    new (end()) T(std::forward<T>(data_[Size() - 1]));
                    std::move_backward(begin() + pos, end() - 1, end());
                    *(begin() + pos) = std::forward<T>(tmp_obj);
                }
                else {
                    new (end()) T(std::forward<Args>(args)...);
                }
            }
            catch (...) {
                operator delete (end());
                throw;
            }
        }
        else {
            RawMemory<T> new_data(Size() == 0 ? 1 : Size() * 2);
            new (new_data.GetAddress() + pos) T(std::forward<Args>(args)...);

            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), pos, new_data.GetAddress());
            }
            else {
                std::uninitialized_copy_n(data_.GetAddress(), pos, new_data.GetAddress());
            }

            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress() + pos, Size() - pos, new_data.GetAddress() + pos + 1);
            }
            else {
                std::uninitialized_copy_n(data_.GetAddress() + pos, Size() - pos, new_data.GetAddress() + pos + 1);
            }

            std::destroy_n(data_.GetAddress(), Size());
            data_.Swap(new_data);
        }
        size_++;
        return begin() + pos;
    }

    iterator Erase(const_iterator position) {
        int pos = position - begin();
        std::move(begin() + pos + 1, end(), begin() + pos);
        std::destroy_at(end() - 1);
        size_--;
        return (begin() + pos);
    }

    iterator Insert(const_iterator position, const T& value) {
        return Emplace(position, value);
    }

    iterator Insert(const_iterator position, T&& value) {
        return Emplace(position, std::move(value));
    }

    iterator begin() noexcept {
        return data_.GetAddress();
    }

    iterator end() noexcept {
        return data_.GetAddress() + Size();
    }

    const_iterator begin() const noexcept {
        return cbegin();
    }

    const_iterator end() const noexcept {
        return cend();
    }

    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }

    const_iterator cend() const noexcept {
        return data_.GetAddress() + Size();
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;



    static void DestroyN(T* buf, size_t n) noexcept {
        for (size_t i = 0; i != n; ++i) {
            Destroy(buf + i);
        }
    }

    // Создаёт копию объекта elem в сырой памяти по адресу buf
    static void CopyConstruct(T* buf, const T& elem) {
        new (buf) T(elem);
    }

    // Вызывает деструктор объекта по адресу buf
    static void Destroy(T* buf) noexcept {
        buf->~T();
    }
};
