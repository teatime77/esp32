/**
 * @file uroa_plot.h
 * @brief 6系列のプロット値をBLE既定MTUに収まる通知へ変換する。
 */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <functional>

namespace uroa {
    /**
     * @brief 送信周期を管理するプロット通知。送信先はコールバックで渡し、同じタスクから呼ぶ。
     * @details BLEの既定MTUに収まる6系列のプロット通知。通信手段はコールバックで渡す。 同じタスクから呼ぶ。値や履歴を保存せず、送信周期だけを管理する。
     */
    class PlotTelemetry {
        public:
        static constexpr size_t Channels = 6;
        static constexpr size_t PacketSize = 20;
        using Sender = std::function<void(const uint8_t *, size_t)>;
        /**
         * @brief 通知のschemaと送信コールバックを保持する。
         * @param schema 0以外の通知形式識別子。
         * @param sender 通知バイト列を送るコールバック。
         */
        PlotTelemetry(uint8_t schema, Sender sender);
        /**
         * @brief 通知レートを設定する。変更後は次のpublish()で送信する。
         * @param hz 通知レート（Hz、0は停止）。
         * @return 0〜20Hzならtrue。0は通知停止。
         */
        bool setRate(uint16_t hz); // 0=停止、1〜20Hz。変更後は次のpublishで送る。
        /**
         * @brief 周期が来たときに6系列を通知し、過去の値は蓄積しない。
         * @param now 呼出元の現在時刻（ms、周回あり）。
         * @param values 6系列の値。エンコード可能な範囲は−327.68〜327.67。
         * @return 送信関数を呼んだらtrue。受信側への到達を保証する値ではない。
         */
        bool publish(uint32_t now, const float (&values)[Channels]);
        /**
         * @brief 6値を0.01単位の符号付き整数へ丸め、20バイトへ詰める。
         * @details 純粋なエンコード処理。非有限値・範囲外は拒否して出力を変更しない。
         * @param out 出力先バッファーまたはJSONオブジェクト。
         * @param schema 0以外の通知形式識別子。
         * @param sequence 16bitで周回するsequence。
         * @param now 呼出元の現在時刻（ms、周回あり）。
         * @param values 6系列の値。エンコード可能な範囲は−327.68〜327.67。
         * @return 変換成功ならtrue。非有限値・範囲外・schema=0は拒否し、出力を変更しない。
         */
        static bool encode(uint8_t (&out)[PacketSize], uint8_t schema, uint16_t sequence, uint32_t now,
            const float (&values)[Channels]);

        private:
        const uint8_t schema_;
        Sender sender_;
        uint16_t rate_ = 0;
        uint16_t sequence_ = 0;
        uint32_t last_ = 0;
        bool sent_ = false;
    };
}
