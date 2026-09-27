// include/core/conversation.h
// Student implementation for ECE 309 Project 2.

#pragma once

#include "core/message.h"

#include <cstddef>

class Conversation {
public:
    Conversation();
    ~Conversation();

    Conversation(const Conversation& other);
    Conversation& operator=(const Conversation& other);

    Conversation(Conversation&& other) noexcept;
    Conversation& operator=(Conversation&& other) noexcept;

    void append(Message m);

    std::size_t size() const noexcept;
    const Message& at(std::size_t i) const;

    const Message* begin() const noexcept;
    const Message* end() const noexcept;

private:
    Message* data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t capacity_ = 0;
};
