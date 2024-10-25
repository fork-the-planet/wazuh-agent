#pragma once

#include <imultitype_queue.hpp>
#include <gmock/gmock.h>

class QueueMock : public IMultiTypeQueue {
public:
    MOCK_METHOD(int, push, (Message message, bool shouldWait), (override));
    MOCK_METHOD(boost::asio::awaitable<int>, pushAwaitable, (Message message), (override));
    MOCK_METHOD(int, push, (std::vector<Message> messages), (override));
    MOCK_METHOD(Message, getNext, (MessageType type, const std::string module), (override));
    MOCK_METHOD(boost::asio::awaitable<Message>, getNextNAwaitable, (MessageType type, int messageQuantity, const std::string moduleName), (override));
    MOCK_METHOD(std::vector<Message>, getNextN, (MessageType type, int messageQuantity, const std::string moduleName), (override));
    MOCK_METHOD(bool, pop, (MessageType type, const std::string moduleName), (override));
    MOCK_METHOD(int, popN, (MessageType type, int messageQuantity, const std::string moduleName), (override));
    MOCK_METHOD(bool, isEmpty, (MessageType type, const std::string moduleName), (override));
    MOCK_METHOD(bool, isFull, (MessageType type, const std::string moduleName), (override));
    MOCK_METHOD(int, storedItems, (MessageType type, const std::string moduleName), (override));
};