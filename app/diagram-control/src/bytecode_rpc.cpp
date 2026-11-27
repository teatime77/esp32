/**
 * @file bytecode_rpc.cpp
 * @brief BLEの登録・受付確認をloop()で処理し、VMの整数通知をキューへコピーする。
 */
#include "bytecode_rpc.h"
#include "bytecode_format.h"
#include <Arduino.h>
#include <cstring>
#include <new>

namespace {
    /**
     * @brief little endianの16bit値を読む。
     * @param data 2バイトの入力。
     * @return 復号した値。
     */
    uint16_t read16(const uint8_t *data) {
        return data[0] | (uint16_t(data[1]) << 8);
    }

    /**
     * @brief little endianの32bit値を読む。
     * @param data 4バイトの入力。
     * @return 復号した値。
     */
    uint32_t read32(const uint8_t *data) {
        return read16(data) | (uint32_t(read16(data + 2)) << 16);
    }

    /**
     * @brief little endianの32bit値を書く。
     * @param data 4バイトの出力。
     * @param value 書き込む値。
     */
    void write32(uint8_t *data, uint32_t value) {
        for (uint8_t index = 0; index < 4; ++index) {
            data[index] = uint8_t(value >> (index * 8));
        }
    }

    /**
     * @brief 登録データのCRC-32/ISO-HDLCを求める。
     * @param data 登録データ。
     * @param length データ長。
     * @return CRC-32。
     */
    uint32_t crc32(const uint8_t *data, size_t length) {
        uint32_t crc = 0xFFFFFFFF;
        for (size_t index = 0; index < length; ++index) {
            crc ^= data[index];
            for (uint8_t bit = 0; bit < 8; ++bit) {
                crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
            }
        }
        return crc ^ 0xFFFFFFFF;
    }
}

/**
 * @copydoc BytecodeRpc::BytecodeRpc
 */
BytecodeRpc::BytecodeRpc(BytecodeVM &vm, const BytecodeVM::NativeApi &base, ControlTransport &transport)
    : vm_(vm), base_(base), transport_(transport) {
}

/**
 * @copydoc BytecodeRpc::begin
 */
bool BytecodeRpc::begin() {
    frames_ = xQueueCreate(4, sizeof(Frame));
    return frames_ != nullptr;
}

/**
 * @copydoc BytecodeRpc::enqueue
 */
void BytecodeRpc::enqueue(const uint8_t *data, size_t length, ControlSession session) {
    if (!frames_ || length < 8 || length > 20) {
        return;
    }
    Frame frame;
    frame.length = uint8_t(length);
    frame.session = session;
    std::memcpy(frame.data, data, length);
    if (xQueueSend(frames_, &frame, 0) != pdTRUE) {
        overflow_.store(true);
    }
}

/**
 * @copydoc BytecodeRpc::stop
 */
void BytecodeRpc::stop() {
    const bool published = nonce_ != 0;
    vm_.stop();
    if (entries_) {
        vm_.bindNativeApi(&base_);
    }
    entries_.reset();
    configured_ = false;
    receiving_ = false;
    used_ = false;
    count_ = 0;
    head_ = 0;
    nonce_ = 0;
    fault_ = Ok;
    if (published) {
        publish();
    }
}

/**
 * @copydoc BytecodeRpc::reset
 */
void BytecodeRpc::reset(void *context) {
    auto &self = *static_cast<BytecodeRpc *>(context);
    if (self.base_.reset) {
        self.base_.reset(self.base_.context);
    }
}

/**
 * @copydoc BytecodeRpc::invoke
 */
BytecodeVM::Error BytecodeRpc::invoke(BytecodeVM::NativeCall &call) {
    auto &self = *static_cast<BytecodeRpc *>(call.context);
    for (size_t index = 0; index < self.base_.count; ++index) {
        const auto &entry = self.base_.entries[index];
        if (entry.id == call.nativeId) {
            call.context = self.base_.context;
            return entry.invoke(call);
        }
    }
    if (!self.configured_ || !self.used_ || self.fault_ != Ok || !self.transport_.isCurrent(self.session_)) {
        return BytecodeVM::Error::NativeUnavailable;
    }
    for (size_t index = 0; index < self.config_[1]; ++index) {
        const uint8_t *binding = self.config_ + 100 + index * 10;
        if (read16(binding) != call.nativeId) {
            continue;
        }
        const uint16_t object = read16(binding + 2);
        const uint16_t method = read16(binding + 4);
        const uint16_t classId = read16(binding + 6);
        if (method == 0) {
            call.results[0].type = BytecodeVM::Type::Object;
            call.results[0].classId = classId;
            call.results[0].address = object;
            return BytecodeVM::Error::None;
        }
        if (call.arguments[0].type != BytecodeVM::Type::Object || call.arguments[0].classId != classId ||
            call.arguments[0].address != object ||
            (binding[8] && call.arguments[1].type != BytecodeVM::Type::Integer)) {
            return BytecodeVM::Error::Type;
        }
        if (self.count_ == MaxRequests || self.nextId_ == UINT32_MAX) {
            self.fault_ = Busy;
            return BytecodeVM::Error::NativeFailure;
        }
        Request &request = self.requests_[(self.head_ + self.count_) % MaxRequests];
        request = {};
        request.id = ++self.nextId_;
        request.time = millis();
        request.object = object;
        request.method = method;
        request.count = binding[8];
        if (request.count) {
            request.value = call.arguments[1].integer;
        }
        ++self.count_;
        call.results[0].type = BytecodeVM::Type::None;
        return BytecodeVM::Error::None;
    }
    return BytecodeVM::Error::NativeUnavailable;
}

/**
 * @copydoc BytecodeRpc::configure
 */
BytecodeRpc::Code BytecodeRpc::configure() {
    if (total_ < 100 || config_[0] != ProtocolVersion || config_[1] == 0 || config_[1] > MaxBindings || config_[2] ||
        config_[3] || total_ != 100 + config_[1] * 10 || crc32(config_, total_) != crc_) {
        return Format;
    }
    if (std::memcmp(config_ + 4, base_.abiHash, 32) != 0) {
        return Mismatch;
    }
    const uint16_t last = base_.count ? base_.entries[base_.count - 1].id : 0;
    for (uint16_t index = 0; index < config_[1]; ++index) {
        const uint8_t *binding = config_ + 100 + index * 10;
        if (uint32_t(last) + index + 1 > UINT16_MAX || read16(binding) != last + index + 1 ||
            read16(binding + 2) == 0 || read16(binding + 6) == 0 || binding[8] > 1 || binding[9]) {
            return Range;
        }
        if (read16(binding + 4) == 0) {
            if (binding[8]) {
                return Format;
            }
        }
        else {
            bool found = false;
            for (uint16_t previous = 0; previous < index; ++previous) {
                const uint8_t *constructor = config_ + 100 + previous * 10;
                if (read16(constructor + 2) == read16(binding + 2) && read16(constructor + 4) == 0 &&
                    read16(constructor + 6) == read16(binding + 6)) {
                    found = true;
                }
            }
            if (!found) {
                return Format;
            }
        }
    }
    entries_.reset(new (std::nothrow) BytecodeVM::NativeEntry[base_.count + config_[1]]);
    if (!entries_) {
        return Busy;
    }
    for (size_t index = 0; index < base_.count; ++index) {
        entries_[index] = base_.entries[index];
        entries_[index].invoke = invoke;
    }
    for (uint16_t index = 0; index < config_[1]; ++index) {
        const uint8_t *binding = config_ + 100 + index * 10;
        entries_[base_.count + index] = {
            read16(binding), uint16_t(read16(binding + 4) ? 1 + binding[8] : 0), 1, invoke};
    }
    api_ = {config_ + 36, entries_.get(), base_.count + config_[1], this, reset};
    configured_ = true;
    return Ok;
}

/**
 * @copydoc BytecodeRpc::bindProgram
 */
bool BytecodeRpc::bindProgram(const uint8_t *data, size_t length, ControlSession session) {
    BytecodeFormat::Image image;
    if (!BytecodeFormat::parseImage(data, length, image)) {
        return false;
    }
    if (std::memcmp(image.abiHash, base_.abiHash, 32) == 0) {
        stop();
        return vm_.bindNativeApi(&base_);
    }
    if (!configured_ || used_ || fault_ != Ok || !transport_.isCurrent(session_) || session.source != session_.source ||
        session.generation != session_.generation || std::memcmp(image.abiHash, api_.abiHash, 32) != 0 ||
        !vm_.bindNativeApi(&api_)) {
        return false;
    }
    used_ = true;
    return true;
}

/**
 * @copydoc BytecodeRpc::result
 */
void BytecodeRpc::result(const Frame &frame, Code code) {
    uint8_t bytes[20]{};
    std::memcpy(bytes, frame.data, 8);
    bytes[8] = code;
    bytes[9] = configured_ ? Configured : receiving_ ? Receiving : Unregistered;
    bytes[10] = uint8_t(received_);
    bytes[11] = uint8_t(received_ >> 8);
    transport_.publishRpcResult(bytes, sizeof bytes, frame.session);
}

/**
 * @copydoc BytecodeRpc::handle
 */
void BytecodeRpc::handle(const Frame &frame) {
    const auto *data = frame.data;
    Code code = Ok;
    const uint16_t sequence = read16(data + 2);
    const uint32_t nonce = read32(data + 4);
    if (data[0] != ProtocolVersion || nonce == 0 || sequence == 0 || frame.session.source != ControlSource::Ble) {
        code = Format;
    }
    else if (data[1] == Begin) {
        if (frame.length != 14 || vm_.state == BytecodeVM::State::Running || vm_.state == BytecodeVM::State::Paused) {
            code = Busy;
        }
        else {
            stop();
            session_ = frame.session;
            nonce_ = nonce;
            sequence_ = sequence;
            total_ = read16(data + 8);
            crc_ = read32(data + 10);
            received_ = 0;
            nextId_ = 0;
            receiving_ = total_ >= 100 && total_ <= MaxConfig;
            code = receiving_ ? Ok : Range;
        }
    }
    else if (nonce != nonce_ || frame.session.generation != session_.generation ||
        sequence != uint32_t(sequence_) + 1) {
        code = Order;
    }
    else {
        sequence_ = sequence;
        if (data[1] == Data && receiving_ && frame.length > 10 && read16(data + 8) == received_ &&
            received_ + frame.length - 10 <= total_) {
            std::memcpy(config_ + received_, data + 10, frame.length - 10);
            received_ += frame.length - 10;
        }
        else if (data[1] == Commit && frame.length == 8 && receiving_ && received_ == total_) {
            code = configure();
            receiving_ = false;
        }
        else if (data[1] == Ack && frame.length == 12 && configured_ && count_ &&
            read32(data + 8) == requests_[head_].id) {
            head_ = (head_ + 1) % MaxRequests;
            --count_;
        }
        else if (data[1] == End && frame.length == 8) {
            stop();
        }
        else {
            code = Order;
        }
    }
    lastFrame_ = millis();
    // ACK・COMMITのResult直後にReadされても、旧先頭・旧世代を返さない順序にする。
    publish();
    result(frame, code);
}

/**
 * @copydoc BytecodeRpc::publish
 */
void BytecodeRpc::publish() {
    uint8_t data[20]{};
    data[0] = ProtocolVersion;
    data[1] = fault_ != Ok ? Fault : count_ ? Pending : Empty;
    write32(data + 2, nonce_);
    if (fault_ != Ok) {
        data[15] = fault_;
    }
    else if (count_) {
        const Request &request = requests_[head_];
        write32(data + 6, request.id);
        data[10] = uint8_t(request.object);
        data[11] = uint8_t(request.object >> 8);
        data[12] = uint8_t(request.method);
        data[13] = uint8_t(request.method >> 8);
        data[14] = request.count;
        write32(data + 16, uint32_t(request.value));
    }
    transport_.publishRpcEvent(data, sizeof data, session_);
}

/**
 * @copydoc BytecodeRpc::process
 */
void BytecodeRpc::process() {
    if (nonce_ && !transport_.isCurrent(session_)) {
        stop();
    }
    if (overflow_.exchange(false)) {
        fault_ = Busy;
        vm_.stop();
    }
    Frame frame;
    for (uint8_t count = 0; frames_ && count < 4 && xQueueReceive(frames_, &frame, 0) == pdTRUE; ++count) {
        if (transport_.isCurrent(frame.session)) {
            handle(frame);
        }
    }
    if ((receiving_ && millis() - lastFrame_ > 2500) || (count_ && millis() - requests_[head_].time > 5000)) {
        if (fault_ == Ok) {
            fault_ = Timeout;
            vm_.stop();
            Serial.println("RPC delivery timeout");
        }
    }
    if (nonce_) {
        publish();
    }
}
