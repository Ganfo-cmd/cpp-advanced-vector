#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>

template <typename T>
class RawMemory
{
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity)), capacity_(capacity)
    {
    }

    RawMemory(const RawMemory &) = delete;
    RawMemory &operator=(const RawMemory &rhs) = delete;

    RawMemory(RawMemory &&other) noexcept
    {
        Swap(other);
    }

    RawMemory &operator=(RawMemory &&rhs) noexcept
    {
        if (this != &rhs)
        {
            Swap(rhs);
        }
        return *this;
    }

    ~RawMemory()
    {
        Deallocate(buffer_);
    }

    T *operator+(size_t offset) noexcept
    {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T *operator+(size_t offset) const noexcept
    {
        return const_cast<RawMemory &>(*this) + offset;
    }

    const T &operator[](size_t index) const noexcept
    {
        return const_cast<RawMemory &>(*this)[index];
    }

    T &operator[](size_t index) noexcept
    {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory &other) noexcept
    {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T *GetAddress() const noexcept
    {
        return buffer_;
    }

    T *GetAddress() noexcept
    {
        return buffer_;
    }

    size_t Capacity() const
    {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T *Allocate(size_t n)
    {
        return n != 0 ? static_cast<T *>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T *buf) noexcept
    {
        operator delete(buf);
    }

    T *buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector
{
public:
    using iterator = T *;
    using const_iterator = const T *;

    Vector() = default;

    explicit Vector(size_t size)
        : data_(size), size_(size)
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector &other)
        : data_(other.size_), size_(other.size_)
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), other.size_, data_.GetAddress());
    }

    Vector(Vector &&other) noexcept
    {
        Swap(other);
    }

    Vector &operator=(const Vector &rhs)
    {
        if (this != &rhs)
        {
            if (rhs.size_ > data_.Capacity())
            {
                Vector new_vector(rhs);
                Swap(new_vector);
            }
            else
            {
                CopyFromRhsVector(rhs);
            }
        }
        return *this;
    }

    Vector &operator=(Vector &&rhs) noexcept
    {
        if (this == &rhs)
        {
            return *this;
        }

        Swap(rhs);
        return *this;
    }

    void Swap(Vector &other) noexcept
    {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    ~Vector()
    {
        std::destroy_n(data_.GetAddress(), size_);
    }

    void Reserve(size_t new_capacity)
    {
        if (new_capacity <= data_.Capacity())
        {
            return;
        }

        RawMemory<T> new_data(new_capacity);
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>)
        {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        else
        {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }

    void Resize(size_t new_size)
    {
        if (new_size < size_)
        {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
        }
        else
        {
            if (new_size > data_.Capacity())
            {
                Reserve(new_size);
            }

            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
        }

        size_ = new_size;
    }

    template <typename U>
    void PushBack(U &&value)
    {
        EmplaceBack(std::forward<U &&>(value));
    }

    template <typename... Args>
    T &EmplaceBack(Args &&...args)
    {
        T *ptr;
        if (size_ >= Capacity())
        {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            ptr = new (new_data.GetAddress() + size_) T(std::forward<Args>(args)...);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>)
            {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            else
            {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }

            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        }
        else
        {
            ptr = new (data_.GetAddress() + size_) T(std::forward<Args>(args)...);
        }

        ++size_;
        return *ptr;
    }

    void PopBack()
    {
        if (size_ != 0)
        {
            --size_;
            std::destroy_at(data_.GetAddress() + size_);
        }
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args &&...args)
    {
        assert(pos >= begin() && pos < end());
        size_t distance = pos - begin();
        if (size_ < data_.Capacity())
        {
            if (pos == end())
            {
                new (end()) T(std::forward<Args>(args)...);
            }
            else
            {
                T temp_obj(std::forward<Args>(args)...);
                new (end()) T(std::move(data_[size_ - 1]));
                std::move_backward(data_.GetAddress() + distance, data_.GetAddress() + size_ - 1, end());
                *(begin() + distance) = std::move(std::forward<T>(temp_obj));
            }
        }
        else
        {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new (new_data.GetAddress() + distance) T(std::forward<Args>(args)...);

            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>)
            {
                std::uninitialized_move_n(begin(), distance, new_data.GetAddress());
                std::uninitialized_move_n(begin() + distance, size_ - distance, new_data.GetAddress() + distance + 1);
            }
            else
            {
                std::uninitialized_copy_n(begin(), distance, new_data.GetAddress());
                std::uninitialized_copy_n(begin() + distance, size_ - distance, new_data.GetAddress() + distance + 1);
            }

            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        }

        ++size_;
        return begin() + distance;
    }

    iterator Insert(const_iterator pos, const T &value)
    {
        return Emplace(pos, value);
    }

    iterator Insert(const_iterator pos, T &&value)
    {
        return Emplace(pos, std::move(value));
    }

    iterator Erase(const_iterator pos)
    {
        assert(pos >= begin() && pos < end());
        size_t shift = pos - begin();
        std::move(begin() + shift + 1, end(), begin() + shift);
        PopBack();
        return begin() + shift;
    }

    size_t Size() const noexcept
    {
        return size_;
    }

    size_t Capacity() const noexcept
    {
        return data_.Capacity();
    }

    const T &operator[](size_t index) const noexcept
    {
        return const_cast<Vector &>(*this)[index];
    }

    T &operator[](size_t index) noexcept
    {
        assert(index < size_);
        return data_[index];
    }

    iterator begin() noexcept
    {
        return data_.GetAddress();
    }

    iterator end() noexcept
    {
        return data_.GetAddress() + size_;
    }

    const_iterator begin() const noexcept
    {
        return data_.GetAddress();
    }

    const_iterator end() const noexcept
    {
        return data_.GetAddress() + size_;
    }

    const_iterator cbegin() const noexcept
    {
        return data_.GetAddress();
    }

    const_iterator cend() const noexcept
    {
        return data_.GetAddress() + size_;
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;

    void CopyFromRhsVector(const Vector &rhs)
    {
        std::copy(rhs.data_.GetAddress(), rhs.data_.GetAddress() + std::min(rhs.size_, size_), data_.GetAddress());

        if (rhs.size_ < size_)
        {
            std::destroy_n(data_.GetAddress() + rhs.size_, size_ - rhs.size_);
        }
        else
        {
            std::uninitialized_copy_n(rhs.data_.GetAddress() + size_, rhs.size_ - size_, data_.GetAddress() + size_);
        }
        size_ = rhs.size_;
    }
};