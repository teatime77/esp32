/**
 * @file control_rpc.generated.h
 * @brief device.protoから生成した通常指令のサービスと応答。手動編集しない。
 */
#pragma once
#include "uroa_rpc.generated.h"
#include "control_session.h"

namespace uroa {
    namespace rpc {
        /**
         * @brief loop()が取得する指令状態。角度は既存cdeg、速度は%で渡す。
         */
        struct ControlStatusState {
            uint8_t flags; ///< 機器と制御の状態。
            uint16_t current; ///< 角度指令のcdeg。
            uint16_t target; ///< 角度目標のcdeg。
            uint16_t remaining; ///< 補間の残りms。
            bool speedMode; ///< 指定CHが速度モードならtrue。
            float speedPercent; ///< 速度指令%。実測速度ではない。
        };

        /**
         * @brief 状態取得・配送と通常形式受理の共通窓口。すべてloop()で呼ぶ。
         */
        struct ControlReplyService {
            ControlStatusState (&snapshot)(uint8_t channel); ///< 指定CHの現在状態。
            void (&publish)(const uint8_t *bytes, size_t length); ///< 完成したStatusを現在の経路へ通知する。
            void (&accepted)(); ///< 通常形式が有効なら細部検査前にheartbeatを更新する。
        };

        /**
         * @brief Servoの型付き機能ハンドラー。単位変換済みの値を受け取る。
         */
        struct ServoCommandService {
            StatusCode (&armAngle)(uint8_t channel, float angle); ///< ARMの機能本体。
            StatusCode (&armSpeed)(uint8_t channel); ///< ARM_SPEEDの機能本体。
            StatusCode (&move)(uint8_t channel, float angle, uint16_t duration); ///< MOVEの機能本体。
            StatusCode (&speed)(uint8_t channel, float percent); ///< SET_SPEEDの機能本体。
            StatusCode (&disable)(uint8_t channel); ///< DISABLEの機能本体。
            StatusCode (&stop)(); ///< STOPの機能本体。
        };

        /**
         * @brief Programの型付き機能ハンドラー。単位変換済みの値を受け取る。
         */
        struct ProgramCommandService {
            StatusCode (&start)(const ControlRpcContext &context); ///< VM_STARTの機能本体。
            StatusCode (&stop)(const ControlRpcContext &context); ///< VM_STOPの機能本体。
        };

        /**
         * @brief Controlの型付き機能ハンドラー。単位変換済みの値を受け取る。
         */
        struct ControlCommandService {
            StatusCode (&stream)(uint16_t rate); ///< STREAMの機能本体。
        };

        /**
         * @brief 稼働中保持するサービス表。必須の関数参照により未登録をコンパイル時に拒否する。
         */
        struct ControlServices {
            ServoCommandService servo; ///< Servoの処理群。
            ProgramCommandService program; ///< Programの処理群。
            ControlCommandService control; ///< Controlの処理群。
            ControlReplyService reply; ///< 共有の状態と配送窓口。
        };

        /**
         * @brief 状態を取得し、通常応答・自発通知のStatusを生成して通知する。
         * @param service 状態取得と通知窓口。
         * @param code 受付結果。
         * @param sequence 応答対象。0は自発通知。
         * @param channel CHまたは全体。
         */
        void publishControlStatus(const ControlReplyService &service, uint8_t code, uint16_t sequence, uint8_t channel);

        /**
         * @brief 復号済みCommandを検査・振分けし、ハンドラーの結果からStatusを返す。
         * @param command 受信時に生成codecで復号した値。
         * @param context loop()の呼出元と停止優先状態。
         * @param services 機能本体と共通の通知窓口。
         * @details 通信コールバックから直接呼ばない。優先STOPは呼出元の従来経路で処理する。
         */
        void dispatchControl(const CommandPacket &command, const ControlRpcContext &context, const ControlServices &services);
    }
}
