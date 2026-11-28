/**
 * @file bytecode_receiver.h
 * @brief 一括転送をmutex付き領域へ蓄積し、loop()で検証・確定する。
 */
#pragma once
#include <uroa_wire.generated.h>
#include <Arduino.h>
#include <functional>
#include <memory>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "control_session.h"
#include "bytecode_format.h"

/**
 * @brief 通信タスクは受信領域の確保・コピーだけ、loop()はCRC・コード・PCの検証と確定を行う。
 * @details 通信仕様の正本はuroa/diagram/ts/ble-wifi/bytecode-transfer.ts先頭。
 * enqueue/process/公開結果はmutexで排他し、I2CとVM操作は通信コールバックで行わない。
 * programとdeploymentの公開メンバーはloop()だけが更新する。VMはprogramを借用する。
 */
class BytecodeReceiver {
    public:
    static constexpr size_t MaxLength = BytecodeFormat::MaxLength; ///< コード全体の上限。共通形式定義に従う。
    static constexpr size_t MaxBundle = uroa::wire::BundleHeaderLayout::ByteLength + MaxLength +
        (uroa::wire::ProgramBundleLimits::Breakpoints + uroa::wire::ProgramBundleLimits::Points) *
            uroa::wire::PcLayout::ByteLength; ///< PC一覧込み上限。
    static constexpr size_t MaxFrame = uroa::wire::TransferHeaderLayout::ByteLength +
        uroa::wire::ProgramFrameLimits::Payload; ///< Wi-Fiの最大フレーム。BLEは合意MTUに制限する。
    std::unique_ptr<uint8_t[]> program; ///< loop()が確定するコードと末尾のPC一覧の所有領域。
    size_t programLength = 0; ///< コード部分だけの長さ。
    uint32_t programCrc32 = 0; ///< フラグ設定前のコードCRC。
    uint32_t programRevision = 0; ///< 正常な全体検証で更新する世代。
    bool deploymentPending = false; ///< loop()がPC設定・開始結果を確定するまでtrue。
    bool startRequested = false; ///< 今回のバンドルの開始フラグ。
    uint32_t breakpointCount = 0; ///< 今回確定した行PC数。
    uint32_t stepCount = 0; ///< 今回確定したステップPC数。
    const uint16_t *breakpoints = nullptr; ///< program末尾の整列済み行PC列。
    const uint16_t *stepPoints = nullptr; ///< program末尾の整列済みステップPC列。

    /**
     * @brief 排他を準備する。
     * @param publish 専用Result出力。
     * @param current 接続世代の照合。
     */
    void begin(const std::function<void(const uint8_t *, size_t, const ControlSession &)> &publish,
        const std::function<bool(const ControlSession &)> &current);
    /**
     * @brief 通信タスクから一括領域へコピーする。
     * @param data フレーム。
     * @param length 長さ。
     * @param session 接続世代。
     */
    void enqueue(const uint8_t *data, size_t length, ControlSession session);
    /**
     * @brief loop()で受信期限・全体検証・確定を行う。
     * @param connected 接続有効。
     * @param session 現接続。
     */
    void process(bool connected, ControlSession session);
    /**
     * @brief loop()から開始成功または保持完了を確定する。
     * @param success PC設定・必要なVM初期化の成否。
     */
    void completeDeployment(bool success);
    /**
     * @brief loop()のStopで未完了受信と開始待ちを取り消す。確定コードは保持する。
     */
    void cancelDeployment();
    /**
     * @brief setup/loop()からResultを公開する。通信層のロックとの逆順取得を避け、mutex外で配送する。
     */
    void publishResult();

    private:
    using Code = uroa::wire::TransferCode::Value; ///< Result.code。
    /**
     * @brief コード全体を検証する。
     * @param data コード。
     * @param length コード長。
     * @return 検証結果。
     */
    Code validate(const uint8_t *data, size_t length);
    /**
     * @brief 一括領域を検証しprogramを置換する。mutexを保持しloop()から呼ぶ。
     * @return 結果。
     */
    Code adopt();
    /**
     * @brief 昇順PC列を命令境界に照合する。
     * @param image 検証済みコード。
     * @param imageLength イメージ全長。
     * @param pcs PC列。
     * @param count 個数。
     * @return 全一致。
     */
    bool validatePcs(const uint8_t *image, size_t imageLength, const uint8_t *pcs, uint32_t count);
    /**
     * @brief mutex保持中に受信を破棄して最初のエラーを保持する。
     * @param code 結果。
     */
    void reject(Code code);
    SemaphoreHandle_t lock_ = nullptr; ///< 受信状態・領域・Resultの排他。
    std::function<void(const uint8_t *, size_t, const ControlSession &)>
        publish_; ///< begin()で登録するResult配送先。受信器のmutexを解放してから呼ぶ。
    std::function<bool(const ControlSession &)> current_; ///< begin()で登録する現接続の照合処理。受信時と確定時に呼ぶ。
    std::unique_ptr<uint8_t[]> pending_; ///< 通信タスクが蓄積する未確定バンドルの所有領域。mutexで保護する。
    ControlSession session_{}; ///< 受信中の通信経路と接続世代。変更時は未完了受信を破棄する。
    uint16_t transferId_ = 0; ///< 現接続で採用した転送ID。新しいBEGINと旧転送を区別する。
    uint16_t sequence_ = 0; ///< 最後に受理したフレームの連番。次の受信が直後の番号かを検査する。
    uint8_t operation_ = 0; ///< 最後に受理した操作番号。publishResult()がResultへ格納する。
    uint8_t state_ = 0; ///< Resultの状態番号。0=待機、1=受信、2=保持完了、3=失敗、4=開始完了。
    Code error_ = Code::Ok; ///< 現在の転送で最初に生じたエラー。reject()が後続エラーによる上書きを防ぐ。
    uint32_t received_ = 0; ///< バンドルへコピー済みのバイト数。DATAのoffsetとCOMMIT時に照合する。
    uint32_t total_ = 0; ///< BEGINで指定されたバンドル全体のバイト数。MaxBundle以下に制限する。
    uint32_t expectedCrc_ = 0; ///< BEGINで指定されたバンドル全体のCRC-32。adopt()が照合する。
    uint32_t lastReceive_ = 0; ///< 最後に受理したBEGIN/DATAのmillis。process()が15秒の期限を監視する。
    bool receiving_ = false; ///< BEGIN後、COMMIT前の段階受信中か。mutex内で更新する。
    bool sealed_ = false; ///< COMMIT受理後の全体検証待ち。loop()のprocess()が取り出して解除する。
    bool dirty_ = true; ///< Resultの再公開が必要か。状態更新時に立て、publishResult()が解除する。
};
