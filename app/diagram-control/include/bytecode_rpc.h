/**
 * @file bytecode_rpc.h
 * @brief BLEの公開定義登録と、VMからブラウザへの整数通知キュー。
 * @details 通信の中心契約はuroa/diagram/ts/ble-wifi/rpc-protocol.ts。
 * 通信コールバックはenqueue()だけを呼び、VM・登録表・要求の操作はloop()へ集約する。
 */
#pragma once
#include <uroa_wire.generated.h>
#include "bytecode_vm.h"
#include "control_transport.h"
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

/**
 * @brief 第3段階の戻り値なし・整数0〜1引数のRPCを管理する。
 * @details 動的表は基本機器APIを包み、基本ID・context・resetを元のバックエンドへ配送する。
 * 要求には整数のコピーだけを保持する。GC参照・VM内部アドレスは持ち越さない。
 */
class BytecodeRpc {
    public:
    /**
     * @brief VM・基本API・通信窓口を稼働中借用する。
     * @param vm loop()で操作するVM。
     * @param base 既存の機器API。
     * @param transport 現接続への配送窓口。
     */
    BytecodeRpc(BytecodeVM &vm, const BytecodeVM::NativeApi &base, ControlTransport &transport);
    /**
     * @brief 通信からloop()へ渡すキューを作る。
     * @return 確保成功ならtrue。
     */
    bool begin();
    /**
     * @brief BLEコールバックのバイト列と接続世代をコピーする。
     * @param data 呼出中だけ有効なフレーム。
     * @param length 8〜20バイト。
     * @param session 受信元と接続世代。
     */
    void enqueue(const uint8_t *data, size_t length, ControlSession session);
    /**
     * @brief loop()で設定・受付確認・接続・期限を処理し、先頭要求を公開する。
     */
    void process();
    /**
     * @brief 新イメージの指紋で基本表または登録済みRPC表を選ぶ。
     * @param data 開始するバイトコード。
     * @param length イメージの長さ。
     * @param session 開始要求の受信元。
     * @return 未登録・旧接続・使用済みRPC設定はfalse。
     * @details 呼出元は先にVMを停止する。RPC設定は1回の開始だけに使用する。
     */
    bool bindProgram(const uint8_t *data, size_t length, ControlSession session);
    /**
     * @brief 明示Stop・切断で要求と動的表を破棄し、基本APIへ戻す。
     */
    void stop();

    private:
    static constexpr uint8_t ProtocolVersion = uroa::wire::RemoteHeaderLayout::versionDefault; ///< BLE専用RPCの版。
    static constexpr size_t MaxBindings =
        uroa::wire::RegistryLimits::Records; ///< コンストラクタも含む動的nativeの上限。
    static constexpr size_t MaxConfig = uroa::wire::RegistryHeaderLayout::ByteLength +
        uroa::wire::RegistryRecordLayout::ByteLength * MaxBindings; ///< 署名・指紋登録の上限。
    static constexpr size_t MaxRequests = 8; ///< ブラウザ受付未確認の要求数上限。

    using Code = uroa::wire::RemoteCode::Value; ///< RPC専用の受付結果。

    /**
     * @brief 通信タスクから渡す最大20バイトのフレーム。
     */
    struct Frame {
        uint8_t data[20]{}; ///< コピーしたフレーム。
        uint8_t length = 0; ///< 有効長。
        ControlSession session{}; ///< 受信元と接続世代。
    };

    /**
     * @brief 送信待ちの整数要求。先頭をACKまで保持する。
     */
    struct Request {
        uint32_t id = 0; ///< セッション内で単調増加する要求ID。
        uint32_t time = 0; ///< キューへの追加時刻。
        uint16_t object = 0; ///< 公開オブジェクトID。
        uint16_t method = 0; ///< 公開メソッドID。
        uint8_t count = 0; ///< 整数引数数。0または1。
        int32_t value = 0; ///< 引数の独立したコピー。
    };

    /**
     * @brief 基本機器APIまたはRPCへnativeIdで配送する。
     * @param call 同期呼出し中だけ有効な入出力。
     * @return 完了またはVMのエラー。
     */
    static BytecodeVM::Error invoke(BytecodeVM::NativeCall &call);
    /**
     * @brief VMのstopから基本バックエンドだけを解放する。設定・送信待ちは明示Stopで破棄する。
     * @param context 登録したBytecodeRpc。
     */
    static void reset(void *context);
    /**
     * @brief 全体検証して動的表を構築する。
     * @return 設定の検証結果。
     */
    Code configure();
    /**
     * @brief 1フレームをloop()で処理する。
     * @param frame 現接続のコピー済み要求。
     */
    void handle(const Frame &frame);
    /**
     * @brief 操作・連番・世代を含むResultを公開する。
     * @param frame 元の操作。
     * @param code 受付結果。
     */
    void result(const Frame &frame, Code code);
    /**
     * @brief 先頭要求または障害を20バイトの専用窓口へ公開する。
     */
    void publish();
    BytecodeVM &vm_; ///< loop()でだけ操作するVM。
    const BytecodeVM::NativeApi &base_; ///< 元の機器登録表。
    ControlTransport &transport_; ///< 接続世代と配送の窓口。
    QueueHandle_t frames_ = nullptr; ///< 通信からのフレームを4件保持する。
    std::atomic<bool> overflow_{false}; ///< 通信側のキュー飽和をloop()へ伝える。
    ControlSession session_{}; ///< 設定を受け付けたBLE接続。
    uint32_t nonce_ = 0; ///< ブラウザが開始ごとに更新するRPC実行世代。
    uint16_t sequence_ = 0; ///< 最後に処理したControl連番。
    uint8_t config_[MaxConfig]{}; ///< 検証後も保持する指紋と動的ID表。
    uint16_t total_ = 0; ///< BEGINで指定された設定長。
    uint16_t received_ = 0; ///< DATAで保持した設定長。
    uint32_t crc_ = 0; ///< 設定全体のCRC-32。
    uint32_t lastFrame_ = 0; ///< 設定受信の期限判定。
    bool receiving_ = false; ///< 設定を受信中か。
    bool configured_ = false; ///< COMMIT済みか。
    bool used_ = false; ///< 設定をVM開始へ使用済みか。
    Code fault_ = Code::Ok; ///< キュー飽和・配送期限等の最初の障害。
    std::unique_ptr<BytecodeVM::NativeEntry[]> entries_; ///< 基本表とRPCを合わせたID昇順の表。
    BytecodeVM::NativeApi api_{}; ///< entries_とconfig_の指紋を借用する動的表。
    Request requests_[MaxRequests]{}; ///< GC参照を持たない要求リング。
    uint8_t head_ = 0; ///< 先頭要求の位置。
    uint8_t count_ = 0; ///< 受付未確認の要求数。
    uint32_t nextId_ = 0; ///< 再利用しない要求ID。
};
