/**
 * @file bytecode_receiver.h
 * @brief BLEの分割バイトコードをキュー経由で受け取り、検証後にRAMへ保持する。
 */
#pragma once
#include <Arduino.h>
#include <atomic>
#include <functional>
#include <memory>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

/**
 * @brief loop()が転送状態・確定プログラムを管理する受信器。VMやFlash保存は行わない。
 * @details BLEタスクはenqueue()だけを呼び、接続世代の検査と動的確保はprocess()で行う。
 */
class BytecodeReceiver {
    public:
    /**
     * @details
     * 転送全体の上限は128KiB（131072バイト）。生成器の「命令列のみ65536バイト」の上限とは別に、ヘッダーと関数情報も含めて制限する。128KiB以内でも空きRAM不足なら受信を拒否する。上限内ではフレーム数が65535未満なので、sequenceは転送中に周回しない。
     */
    static constexpr size_t MaxLength = 128 * 1024; ///< ヘッダー・関数表を含む転送全体の最大バイト数。
    std::unique_ptr<uint8_t[]> program; ///< loop()だけが正常COMMIT時に置換する。外部は参照だけ行う。
    size_t programLength = 0; ///< 確定済みプログラム長。切断で消去せず、再起動で失われる。
    uint32_t programCrc32 = 0; ///< 確定済みプログラム全体のCRC-32。
    uint32_t programRevision = 0; ///< 正常COMMITで更新する世代。loop()はVM実行前に照合する。

    /**
     * @brief 受信キューを確保する。
     * @param publish 受付結果20バイトの出力先。setup()とloop()から呼ぶ。
     * @param current 確定直前にも接続世代を再照合する関数。loop()から呼ぶ。
     */
    void begin(const std::function<void(const uint8_t *, size_t)> &publish,
        const std::function<bool(uint32_t)> &current);
    /** @brief BLEタスクからコピーする。@param data フレーム。@param length 長さ。@param session 接続世代。 */
    void enqueue(const uint8_t *data, size_t length, uint32_t session);
    /** @brief 現接続のフレームと期限を処理する。@param connected BLE利用中か。@param session 接続世代。 */
    void process(bool connected, uint32_t session);
    /** @brief サービス公開前にも呼べる、現在の受付結果の再公開。 */
    void publishResult();

    private:
    /** @brief 受信結果コード。diagramのbytecode-transfer.tsと共通。
     *
     * @details
     * 結果コードは0=成功、1=フレーム形式不正、2=ID・順序・位置不正、3=長さ範囲外、4=メモリ不足、5=CRC不一致、6=バイトコード形式不正、7=受信期限切れ、8=受信キュー混雑。BEGIN／DATA成功の状態はreceiving、COMMIT成功の状態はready。エラー時の各長さは診断情報で、保持完了を表さない。転送前・接続変更時の結果はIDとsequenceが0で、進行中のフレームへのACKとして扱わない。
     */
    enum Code : uint8_t {
        Ok = 0,
        Format = 1,
        Order = 2,
        Range = 3,
        Memory = 4,
        Crc = 5,
        Invalid = 6,
        Timeout = 7,
        Busy = 8
    };
    /** @brief BLEタスクからloop()へ渡す固定長コピー。 */
    struct Frame {
        uint32_t session = 0;
        uint8_t length = 0;
        uint8_t data[20]{};
    };

    /** @brief フレームを検査して状態を更新する。@param frame 現接続世代のフレーム。 */
    void handle(const Frame &frame);
    /** @brief 未完了バッファを破棄する。確定済みプログラムは保持する。 */
    void cancel();
    /** @brief 結果を更新する。@param code 受付結果。@param frame 応答対象。nullなら直前の番号を使う。 */
    void reply(Code code, const Frame *frame = nullptr);
    /** @brief バイナリ形式を検証する。@param data 全データ。@param length 長さ。@return 結果コード。 */
    Code validate(const uint8_t *data, size_t length);

    QueueHandle_t frames_ = nullptr; ///< setup()で確保し、稼働中は保持する。
    std::atomic<bool> overflow_{false}; ///< BLEタスクが設定し、loop()が解除する。
    std::function<void(const uint8_t *, size_t)> publish_;
    std::function<bool(uint32_t)> current_;
    std::unique_ptr<uint8_t[]> pending_;
    uint8_t result_[20]{};
    uint32_t session_ = 0;
    bool connected_ = false;
    bool receiving_ = false;
    uint16_t transferId_ = 0;
    uint16_t sequence_ = 0;
    uint32_t received_ = 0;
    uint32_t total_ = 0;
    uint32_t expectedCrc_ = 0;
    uint32_t crc_ = 0xFFFFFFFF;
    uint32_t lastReceive_ = 0;
};
