// src/conversation.cpp
// Student implementation for ECE 309 Project 2.

#include "core/conversation.h"

#include <limits>
#include <stdexcept>
#include <utility>

Conversation::Conversation() = default;

Conversation::~Conversation() {
    delete[] data_;
}

Conversation::Conversation(const Conversation& other) {
    if (other.capacity_ == 0) return;

    Message* copied_data = new Message[other.capacity_];
    try {
        for (std::size_t i = 0; i < other.size_; ++i) {
            copied_data[i] = other.data_[i];
        }
    } catch (...) {
        delete[] copied_data;
        throw;
    }

    data_ = copied_data;
    size_ = other.size_;
    capacity_ = other.capacity_;
}

Conversation& Conversation::operator=(const Conversation& other) {
    if (this == &other) return *this;

    // Construct first so a failed allocation or Message copy leaves this
    // conversation unchanged. The temporary then owns the old buffer.
    Conversation copy(other);
    using std::swap;
    swap(data_, copy.data_);
    swap(size_, copy.size_);
    swap(capacity_, copy.capacity_);
    return *this;
}

Conversation::Conversation(Conversation&& other) noexcept
    : data_(other.data_), size_(other.size_), capacity_(other.capacity_) {
    other.data_ = nullptr;
    other.size_ = 0;
    other.capacity_ = 0;
}

Conversation& Conversation::operator=(Conversation&& other) noexcept {
    if (this == &other) return *this;

    delete[] data_;
    data_ = other.data_;
    size_ = other.size_;
    capacity_ = other.capacity_;

    other.data_ = nullptr;
    other.size_ = 0;
    other.capacity_ = 0;
    return *this;
}

void Conversation::append(Message m) {
    if (size_ == capacity_) {
        std::size_t new_capacity = 1;
        if (capacity_ != 0) {
            if (capacity_ > std::numeric_limits<std::size_t>::max() / 2) {
                throw std::length_error("Conversation capacity overflow");
            }
            new_capacity = capacity_ * 2;
        }

        Message* expanded_data = new Message[new_capacity];
        try {
            // Copy existing entries before committing the new backing store so
            // an exception leaves the old conversation fully intact.
            for (std::size_t i = 0; i < size_; ++i) {
                expanded_data[i] = data_[i];
            }
            expanded_data[size_] = std::move(m);
        } catch (...) {
            delete[] expanded_data;
            throw;
        }

        delete[] data_;
        data_ = expanded_data;
        capacity_ = new_capacity;
    } else {
        data_[size_] = std::move(m);
    }

    ++size_;
}

std::size_t Conversation::size() const noexcept {
    return size_;
}

const Message& Conversation::at(std::size_t i) const {
    if (i >= size_) {
        throw std::out_of_range("Conversation::at index out of range");
    }
    return data_[i];
}

const Message* Conversation::begin() const noexcept {
    return data_;
}

const Message* Conversation::end() const noexcept {
    return data_ == nullptr ? nullptr : data_ + size_;
}
