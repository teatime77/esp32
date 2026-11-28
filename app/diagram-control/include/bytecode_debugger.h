/**
 * @file bytecode_debugger.h
 * @brief BLE／Wi-Fiデバッグ要求のキュー、命令フラグと停止Snapshotをloop()で管理する。
 */
#pragma once
#include <uroa_wire.generated.h>
#include <Arduino.h>
#include <atomic>
#include <memory>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "control_transport.h"
#include "bytecode_receiver.h"
#include "bytecode_vm.h"

/**
 * @brief 通信仕様はuroa/diagram/ts/ble-wifi/debug-protocol.tsを正本とする。
 */
class BytecodeDebugger {
    public:
    /**
     * @brief 稼働中保持する参照を接続する。
     * @param receiver プログラム所有者。
     * @param vm VM。
     * @param transport 通信共通窓口。
     */
    BytecodeDebugger(BytecodeReceiver &receiver, BytecodeVM &vm, ControlTransport &transport);
    /**
     * @brief setup()で固定要求キューを確保する。
     */
    void begin();
    /**
     * @brief 通信タスクで要求をコピーする。
     * @param data 要求。
     * @param length 8〜20。
     * @param session 受信元と接続世代。
     */
    void enqueue(const uint8_t *data, size_t length, ControlSession session);
    /**
     * @brief loop()で接続・プログラム世代と要求を処理する。
     * @param enabled 現接続で処理可能か。
     * @param session 受信元と接続世代。
     */
    void process(bool enabled, ControlSession session);
    /**
     * @brief tick()後に状態を通知し、Pausedへの遷移では不変Snapshotを確保する。
     */
    void publishState();
    /**
     * @brief VMの開始・停止前に旧Snapshotを無効化する。命令フラグは同じプログラムに保持する。
     */
    void invalidateSnapshot();
    /**
     * @brief loop()で一括転送の検証済みPC列を設定する。
     * @return ステップ表の確保と設定に成功したらtrue。
     */
    bool installDeployment();

    private:
    /**
     * @brief 専用ACKの結果番号。TypeScript側のdebugProtocol.errorsと共通。
     */
    using Code = uroa::wire::DebugCode::Value;

    /**
     * @brief 通信タスクから渡す固定長のコピー。
     */
    struct Request {
        ControlSession session{}; ///< 通信タスクが要求をコピーしたときの経路と接続世代。
        uint8_t length = 0; ///< bytes内の有効バイト数。enqueue()が8〜20バイトに制限する。
        uint8_t bytes[uroa::wire::DebugHeaderLayout::ByteLength +
            uroa::wire::DebugFrameLimits::Payload]{}; ///< 通信コールバックの入力からコピーした固定長の要求領域。
    };

    /**
     * @brief 要求を検査して処理する。
     * @param request 現接続の要求。
     */
    void handle(const Request &request);
    /**
     * @brief 一覧を検証し、行ブレークポイントまたはステップ用停止点を全置換する。
     * @return 全検証成功時true。
     */
    bool applyBreakpoints();
    /**
     * @brief 現在の保持イメージから命令列を求める。
     * @param length 命令列長の出力。
     * @return 命令列。
     */
    uint8_t *code(uint32_t &length);
    /**
     * @brief 現在のイメージの命令先頭からbit7だけを除く。
     */
    void clearFlags();
    /**
     * @brief 段階転送を破棄する。確定済みフラグは維持する。
     */
    void cancelTransfer();
    /**
     * @brief 要求ACKを作る。
     * @param code 結果。
     * @param value 操作別値。
     * @param extra 操作別値。
     */
    void reply(Code code, uint32_t value = 0, uint32_t extra = 0);

    BytecodeReceiver &receiver_; ///< 稼働中借用する受信器。確定コード・プログラム世代を参照する。
    BytecodeVM &vm_; ///< loop()で停止点の設定・再開・Snapshot取得を行うVMの借用参照。
    ControlTransport &transport_; ///< 現接続の照合とデバッグ結果の配送を委譲する借用参照。
    QueueHandle_t requests_ = nullptr; ///< setup()が確保し、稼働中保持する。
    std::atomic<bool> overflow_{false}; ///< enqueue()が設定しprocess()が解除する。
    bool enabled_ = false; ///< process()が更新する、現接続でデバッグ要求を処理できる状態。
    ControlSession session_{}; ///< process()が採用した通信経路と接続世代。返信と失効判定に使う。
    uint32_t revision_ = 0; ///< 命令フラグと停止点を設定したプログラム世代。process()が照合する。
    uint16_t requestId_ = 0; ///< 現在の接続で最後に受理した要求ID。handle()が単調増加を検査する。
    uint8_t operation_ = 0; ///< 直前に受理したデバッグ操作番号。reply()がACKへ格納する。
    uint8_t event_[uroa::wire::DebugEventLayout::
            ByteLength]{}; ///< 最後に配送した20バイトの状態イベント。publishState()が重複を除く。
    uint32_t lastProgressMs_ = 0; ///< RunningのPC・命令数を最大1秒に1回公開する。
    bool receiving_ = false; ///< PC一覧の段階受信中か。handle()が開始し、確定・取消・期限切れで解除する。
    bool pointsTransfer_ = false; ///< 7/8/9ではステップ停止点、それ以外は行ブレークポイント。
    std::unique_ptr<uint16_t[]> pending_; ///< 確定前のPC列の所有領域。handle()が受信し、cancelTransfer()が解放する。
    uint32_t count_ = 0; ///< 開始要求で指定されたPC総数。行停止点とステップ停止点で上限が異なる。
    uint32_t received_ = 0; ///< pending_へ順番にコピー済みのPC数。次の受信位置の検査にも使う。
    uint32_t lastReceive_ = 0; ///< 直前のPC一覧受信時刻（millis）。process()が5秒の期限を監視する。
    uint32_t stopId_ = 0; ///< 接続をまたいでも再利用しない。周回時は取得を拒否する。
    bool snapshotAttempted_ = false; ///< 現在の一時停止についてSnapshot確保を一度試したか。再試行を抑止する。
    std::unique_ptr<uint8_t[]> snapshot_; ///< 一時停止時の不変Snapshotの所有領域。再開・Stop・接続変更で破棄する。
    uint32_t snapshotLength_ = 0; ///< snapshot_の有効バイト数。取得要求のoffset検査に使う。
    Code snapshotError_ = Code::Ok; ///< Snapshotの確保・直列化結果。invalidateSnapshot()でOkへ戻す。
};
