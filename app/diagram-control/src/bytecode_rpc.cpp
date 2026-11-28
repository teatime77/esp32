/**
 * @file bytecode_rpc.cpp
 * @brief BLEの登録・受付確認をloop()で処理し、VMの整数通知をキューへコピーする。
 * @details 登録表・要求・Result/Eventはwire.protoの生成codecを使う。
 * 署名の照合・所有キュー・nonce・接続世代とWorker受付ACKの契約は本ファイルとrpc-protocol.tsが管理する。
 */
#include <uroa_wire.generated.h>
#include "bytecode_rpc.h"
#include "bytecode_format.h"
#include <Arduino.h>
#include <cstring>
#include <new>

namespace {

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
    const auto registry = uroa::wire::readRegistryHeader(self.config_);
    for (size_t index = 0; index < registry.count; ++index) {
        const auto binding = uroa::wire::readRegistryRecord(self.config_ +
            uroa::wire::RegistryHeaderLayout::ByteLength + index * uroa::wire::RegistryRecordLayout::ByteLength);
        if (binding.native != call.nativeId) {
            continue;
        }
        const uint16_t object = binding.object;
        const uint16_t method = binding.method;
        const uint16_t classId = binding.classId;
        if (method == 0) {
            call.results[0].type = BytecodeVM::Type::Object;
            call.results[0].classId = classId;
            call.results[0].address = object;
            return BytecodeVM::Error::None;
        }
        if (call.arguments[0].type != BytecodeVM::Type::Object || call.arguments[0].classId != classId ||
            call.arguments[0].address != object ||
            (binding.argc && call.arguments[1].type != BytecodeVM::Type::Integer)) {
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
        request.count = binding.argc;
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
    const auto header = uroa::wire::readRegistryHeader(config_);
    if (total_ < 100 || header.version != ProtocolVersion || header.count == 0 || header.count > MaxBindings ||
        header.reserved || total_ != 100 + header.count * 10 || crc32(config_, total_) != crc_) {
        return Format;
    }
    if (std::memcmp(header.baseHash, base_.abiHash, 32) != 0) {
        return Mismatch;
    }
    const uint16_t last = base_.count ? base_.entries[base_.count - 1].id : 0;
    for (uint16_t index = 0; index < header.count; ++index) {
        const auto binding = uroa::wire::readRegistryRecord(config_ + uroa::wire::RegistryHeaderLayout::ByteLength +
            index * uroa::wire::RegistryRecordLayout::ByteLength);
        if (uint32_t(last) + index + 1 > UINT16_MAX || binding.native != last + index + 1 || binding.object == 0 ||
            binding.classId == 0 || binding.argc > 1 || binding.reserved) {
            return Range;
        }
        if (binding.method == 0) {
            if (binding.argc) {
                return Format;
            }
        }
        else {
            bool found = false;
            for (uint16_t previous = 0; previous < index; ++previous) {
                const auto constructor =
                    uroa::wire::readRegistryRecord(config_ + uroa::wire::RegistryHeaderLayout::ByteLength +
                        previous * uroa::wire::RegistryRecordLayout::ByteLength);
                if (constructor.object == binding.object && constructor.method == 0 &&
                    constructor.classId == binding.classId) {
                    found = true;
                }
            }
            if (!found) {
                return Format;
            }
        }
    }
    entries_.reset(new (std::nothrow) BytecodeVM::NativeEntry[base_.count + header.count]);
    if (!entries_) {
        return Busy;
    }
    for (size_t index = 0; index < base_.count; ++index) {
        entries_[index] = base_.entries[index];
        entries_[index].invoke = invoke;
    }
    for (uint16_t index = 0; index < header.count; ++index) {
        const auto binding = uroa::wire::readRegistryRecord(config_ + uroa::wire::RegistryHeaderLayout::ByteLength +
            index * uroa::wire::RegistryRecordLayout::ByteLength);
        entries_[base_.count + index] = {binding.native, uint16_t(binding.method ? 1 + binding.argc : 0), 1, invoke};
    }
    api_ = {header.rpcHash, entries_.get(), base_.count + header.count, this, reset};
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
    const auto header = uroa::wire::readRemoteHeader(frame.data);
    const uroa::wire::RemoteResult packet{header.version, header.op, header.sequence, header.nonce,
        static_cast<uint8_t>(code),
        static_cast<uint8_t>(configured_ ? Configured
                : receiving_             ? Receiving
                                         : Unregistered),
        received_, 0, 0};
    uint8_t bytes[uroa::wire::RemoteResultLayout::ByteLength];
    uroa::wire::writeRemoteResult(bytes, packet);
    transport_.publishRpcResult(bytes, sizeof bytes, frame.session);
}

/**
 * @copydoc BytecodeRpc::handle
 */
void BytecodeRpc::handle(const Frame &frame) {
    const auto *data = frame.data;
    Code code = Ok;
    const auto header = uroa::wire::readRemoteHeader(data);
    const uint16_t sequence = header.sequence;
    const uint32_t nonce = header.nonce;
    if (header.version != ProtocolVersion || nonce == 0 || sequence == 0 ||
        frame.session.source != ControlSource::Ble) {
        code = Format;
    }
    else if (header.op == Begin) {
        if (frame.length != 14 || vm_.state == BytecodeVM::State::Running || vm_.state == BytecodeVM::State::Paused) {
            code = Busy;
        }
        else {
            stop();
            session_ = frame.session;
            nonce_ = nonce;
            sequence_ = sequence;
            const auto begin = uroa::wire::readRemoteBegin(data);
            total_ = begin.total;
            crc_ = begin.crc;
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
        if (header.op == Data && receiving_ && frame.length > 10 &&
            uroa::wire::readRemoteData(data).offset == received_ && received_ + frame.length - 10 <= total_) {
            std::memcpy(config_ + received_, data + 10, frame.length - 10);
            received_ += frame.length - 10;
        }
        else if (header.op == Commit && frame.length == 8 && receiving_ && received_ == total_) {
            code = configure();
            receiving_ = false;
        }
        else if (header.op == Ack && frame.length == 12 && configured_ && count_ &&
            uroa::wire::readRemoteAck(data).id == requests_[head_].id) {
            head_ = (head_ + 1) % MaxRequests;
            --count_;
        }
        else if (header.op == End && frame.length == 8) {
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
    uroa::wire::RemoteEvent event{};
    event.version = ProtocolVersion;
    event.kind = fault_ != Ok ? Fault : count_ ? Pending : Empty;
    event.nonce = nonce_;
    if (fault_ != Ok) {
        event.error = fault_;
    }
    else if (count_) {
        const Request &request = requests_[head_];
        event.id = request.id;
        event.object = request.object;
        event.method = request.method;
        event.argc = request.count;
        event.value = request.value;
    }
    uint8_t data[uroa::wire::RemoteEventLayout::ByteLength];
    uroa::wire::writeRemoteEvent(data, event);
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
