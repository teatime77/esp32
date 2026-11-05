/** @file bytecode_debugger.h
 * @brief BLEデバッグ要求のキュー、命令フラグと停止Snapshotをloop()で管理する。
 */
#pragma once
#include <Arduino.h>
#include <atomic>
#include <memory>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <uroa_ble.h>
#include "bytecode_receiver.h"
#include "bytecode_vm.h"

/** @brief 通信仕様はuroa/diagram/ts/ble-wifi/debug-protocol.tsを正本とする。 */
class BytecodeDebugger {
    public:
    /** @brief 稼働中保持する参照を接続する。@param receiver プログラム所有者。@param vm VM。@param ble BLE。 */
    BytecodeDebugger(BytecodeReceiver &receiver, BytecodeVM &vm, uroa::BleTransport &ble);
    /** @brief setup()で固定要求キューを確保する。 */
    void begin();
    /** @brief BLEタスクで要求をコピーする。@param data 要求。@param length 8〜20。@param session 接続世代。 */
    void enqueue(const uint8_t *data, size_t length, uint32_t session);
    /** @brief loop()で接続・プログラム世代と要求を処理する。@param enabled BLE制御中か。@param session 接続世代。 */
    void process(bool enabled, uint32_t session);
    /** @brief tick()後に状態を通知し、Pausedへの遷移では不変Snapshotを確保する。 */
    void publishState();
    /** @brief VMの開始・停止前に旧Snapshotを無効化する。命令フラグは同じプログラムに保持する。 */
    void invalidateSnapshot();

    private:
    /** @brief 専用ACKの結果番号。TypeScript側のdebugProtocol.errorsと共通。 */
    enum Code : uint8_t { Ok, Format, Order, Range, Memory, Crc, Invalid, Busy, Stale, Timeout };
    /** @brief BLEタスクから渡す固定長のコピー。 */
    struct Request {
        uint32_t session = 0;
        uint8_t length = 0;
        uint8_t bytes[20]{};
    };
    /** @brief 要求を検査して処理する。@param request 現接続の要求。 */
    void handle(const Request &request);
    /** @brief 検証済み一覧へ命令フラグを全置換する。@return 全検証成功時true。 */
    bool applyBreakpoints();
    /** @brief 現在の保持イメージから命令列を求める。@param length 命令列長の出力。@return 命令列。 */
    uint8_t *code(uint32_t &length);
    /** @brief 現在のイメージの命令先頭からbit7だけを除く。 */
    void clearFlags();
    /** @brief 段階転送を破棄する。確定済みフラグは維持する。 */
    void cancelTransfer();
    /** @brief 要求ACKを作る。@param code 結果。@param value 操作別値。@param extra 操作別値。 */
    void reply(Code code, uint32_t value = 0, uint32_t extra = 0);

    BytecodeReceiver &receiver_;
    BytecodeVM &vm_;
    uroa::BleTransport &ble_;
    QueueHandle_t requests_ = nullptr; ///< setup()が確保し、稼働中保持する。
    std::atomic<bool> overflow_{false}; ///< enqueue()が設定しprocess()が解除する。
    bool enabled_ = false;
    uint32_t session_ = 0;
    uint32_t revision_ = 0;
    Request lastRequest_;
    uint16_t requestId_ = 0;
    uint8_t operation_ = 0;
    uint8_t result_[20]{};
    uint8_t event_[20]{};
    bool receiving_ = false;
    std::unique_ptr<uint16_t[]> pending_;
    uint32_t count_ = 0;
    uint32_t received_ = 0;
    uint32_t expectedCrc_ = 0;
    uint32_t lastReceive_ = 0;
    uint32_t stopId_ = 0; ///< 接続をまたいでも再利用しない。周回時は取得を拒否する。
    bool snapshotAttempted_ = false;
    std::unique_ptr<uint8_t[]> snapshot_;
    uint32_t snapshotLength_ = 0;
    uint32_t snapshotCrc_ = 0;
    Code snapshotError_ = Ok;
};
