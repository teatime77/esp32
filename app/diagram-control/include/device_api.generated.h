/** @file device_api.generated.h
 * @brief device.pyiから自動生成した値型・機器バックエンドの契約。直接編集しない。
 */
#pragma once
#include "bytecode_vm.h"

namespace device_api {
    /** @brief 呼出し中だけ有効な長さ付きUTF-8。埋込NULを許す。 */
    struct StringView {
        const char *data;
        size_t length;
    };

    constexpr uint16_t ClassIdVec3 = 1; ///< device.pyiのクラスID。
    constexpr uint16_t ClassIdIMUValue = 2; ///< device.pyiのクラスID。
    constexpr uint16_t ClassIdServo = 3; ///< device.pyiのクラスID。
    constexpr uint16_t ClassIdIMU = 4; ///< device.pyiのクラスID。
    constexpr uint16_t ClassIdUltrasonicDistanceSensor = 5; ///< device.pyiのクラスID。
    constexpr uint16_t ClassIdDisplay = 6; ///< device.pyiのクラスID。

    /** @brief 宣言順の値フィールド。VMでは葉の連続スロットへ展開する。 */
    struct Vec3 {
        float x;
        float y;
        float z;
    };

    /** @brief 宣言順の値フィールド。VMでは葉の連続スロットへ展開する。 */
    struct IMUValue {
        Vec3 acc;
        Vec3 gyro;
    };

    /** @brief loop()で同期実行する機器操作。生成スタブが引数を検査して呼ぶ。 */
    class Backend {
        public:
        /** @brief 派生バックエンドを破棄する。 */
        virtual ~Backend();
        /** @brief VM所有の出力を停止し、ハンドルをすべて無効化する。 */
        virtual void reset() = 0;
        /** @brief Servo_constructを実行する。
         * @param arg_channel 検査済み引数。recordは値コピー、deviceは不透明ハンドル。
         * @param arg_mode 検査済み引数。recordは値コピー、deviceは不透明ハンドル。
         * @param result 成功時の戻り値。
         * @return None=成功、NativePending=再試行、その他は実行エラー。
         */
        virtual BytecodeVM::Error Servo_construct(int32_t arg_channel, int32_t arg_mode, uint32_t &result) = 0;
        /** @brief Servo_enableを実行する。
         * @param receiver 検査済み引数。recordは値コピー、deviceは不透明ハンドル。
         * @return None=成功、NativePending=再試行、その他は実行エラー。
         */
        virtual BytecodeVM::Error Servo_enable(uint32_t receiver) = 0;
        /** @brief Servo_disableを実行する。
         * @param receiver 検査済み引数。recordは値コピー、deviceは不透明ハンドル。
         * @return None=成功、NativePending=再試行、その他は実行エラー。
         */
        virtual BytecodeVM::Error Servo_disable(uint32_t receiver) = 0;
        /** @brief Servo_set_angleを実行する。
         * @param receiver 検査済み引数。recordは値コピー、deviceは不透明ハンドル。
         * @param arg_angle 検査済み引数。recordは値コピー、deviceは不透明ハンドル。
         * @return None=成功、NativePending=再試行、その他は実行エラー。
         */
        virtual BytecodeVM::Error Servo_set_angle(uint32_t receiver, float arg_angle) = 0;
        /** @brief Servo_set_speedを実行する。
         * @param receiver 検査済み引数。recordは値コピー、deviceは不透明ハンドル。
         * @param arg_speed 検査済み引数。recordは値コピー、deviceは不透明ハンドル。
         * @return None=成功、NativePending=再試行、その他は実行エラー。
         */
        virtual BytecodeVM::Error Servo_set_speed(uint32_t receiver, float arg_speed) = 0;
        /** @brief IMU_constructを実行する。
         * @param result 成功時の戻り値。
         * @return None=成功、NativePending=再試行、その他は実行エラー。
         */
        virtual BytecodeVM::Error IMU_construct(uint32_t &result) = 0;
        /** @brief IMU_get_valueを実行する。
         * @param receiver 検査済み引数。recordは値コピー、deviceは不透明ハンドル。
         * @param result 成功時の戻り値。
         * @return None=成功、NativePending=再試行、その他は実行エラー。
         */
        virtual BytecodeVM::Error IMU_get_value(uint32_t receiver, IMUValue &result) = 0;
        /** @brief UltrasonicDistanceSensor_constructを実行する。
         * @param result 成功時の戻り値。
         * @return None=成功、NativePending=再試行、その他は実行エラー。
         */
        virtual BytecodeVM::Error UltrasonicDistanceSensor_construct(uint32_t &result) = 0;
        /** @brief UltrasonicDistanceSensor_get_distanceを実行する。
         * @param receiver 検査済み引数。recordは値コピー、deviceは不透明ハンドル。
         * @param result 成功時の戻り値。
         * @return None=成功、NativePending=再試行、その他は実行エラー。
         */
        virtual BytecodeVM::Error UltrasonicDistanceSensor_get_distance(uint32_t receiver, float &result) = 0;
        /** @brief Display_constructを実行する。
         * @param result 成功時の戻り値。
         * @return None=成功、NativePending=再試行、その他は実行エラー。
         */
        virtual BytecodeVM::Error Display_construct(uint32_t &result) = 0;
        /** @brief Display_set_text_colorを実行する。
         * @param receiver 検査済み引数。recordは値コピー、deviceは不透明ハンドル。
         * @param arg_color 検査済み引数。recordは値コピー、deviceは不透明ハンドル。
         * @return None=成功、NativePending=再試行、その他は実行エラー。
         */
        virtual BytecodeVM::Error Display_set_text_color(uint32_t receiver, int32_t arg_color) = 0;
        /** @brief Display_set_cursorを実行する。
         * @param receiver 検査済み引数。recordは値コピー、deviceは不透明ハンドル。
         * @param arg_x 検査済み引数。recordは値コピー、deviceは不透明ハンドル。
         * @param arg_y 検査済み引数。recordは値コピー、deviceは不透明ハンドル。
         * @return None=成功、NativePending=再試行、その他は実行エラー。
         */
        virtual BytecodeVM::Error Display_set_cursor(uint32_t receiver, int32_t arg_x, int32_t arg_y) = 0;
        /** @brief Display_printを実行する。
         * @param receiver 検査済み引数。recordは値コピー、deviceは不透明ハンドル。
         * @param arg_text 検査済み引数。recordは値コピー、deviceは不透明ハンドル。
         * @return None=成功、NativePending=再試行、その他は実行エラー。
         */
        virtual BytecodeVM::Error Display_print(uint32_t receiver, StringView arg_text) = 0;
        /** @brief Display_draw_lineを実行する。
         * @param receiver 検査済み引数。recordは値コピー、deviceは不透明ハンドル。
         * @param arg_start_x 検査済み引数。recordは値コピー、deviceは不透明ハンドル。
         * @param arg_start_y 検査済み引数。recordは値コピー、deviceは不透明ハンドル。
         * @param arg_end_x 検査済み引数。recordは値コピー、deviceは不透明ハンドル。
         * @param arg_end_y 検査済み引数。recordは値コピー、deviceは不透明ハンドル。
         * @param arg_color 検査済み引数。recordは値コピー、deviceは不透明ハンドル。
         * @return None=成功、NativePending=再試行、その他は実行エラー。
         */
        virtual BytecodeVM::Error Display_draw_line(uint32_t receiver, int32_t arg_start_x, int32_t arg_start_y, int32_t arg_end_x, int32_t arg_end_y, int32_t arg_color) = 0;
        /** @brief Display_draw_rectを実行する。
         * @param receiver 検査済み引数。recordは値コピー、deviceは不透明ハンドル。
         * @param arg_left 検査済み引数。recordは値コピー、deviceは不透明ハンドル。
         * @param arg_top 検査済み引数。recordは値コピー、deviceは不透明ハンドル。
         * @param arg_width 検査済み引数。recordは値コピー、deviceは不透明ハンドル。
         * @param arg_height 検査済み引数。recordは値コピー、deviceは不透明ハンドル。
         * @param arg_color 検査済み引数。recordは値コピー、deviceは不透明ハンドル。
         * @return None=成功、NativePending=再試行、その他は実行エラー。
         */
        virtual BytecodeVM::Error Display_draw_rect(uint32_t receiver, int32_t arg_left, int32_t arg_top, int32_t arg_width, int32_t arg_height, int32_t arg_color) = 0;
        /** @brief Display_draw_circleを実行する。
         * @param receiver 検査済み引数。recordは値コピー、deviceは不透明ハンドル。
         * @param arg_center_x 検査済み引数。recordは値コピー、deviceは不透明ハンドル。
         * @param arg_center_y 検査済み引数。recordは値コピー、deviceは不透明ハンドル。
         * @param arg_radius 検査済み引数。recordは値コピー、deviceは不透明ハンドル。
         * @param arg_color 検査済み引数。recordは値コピー、deviceは不透明ハンドル。
         * @return None=成功、NativePending=再試行、その他は実行エラー。
         */
        virtual BytecodeVM::Error Display_draw_circle(uint32_t receiver, int32_t arg_center_x, int32_t arg_center_y, int32_t arg_radius, int32_t arg_color) = 0;
        /** @brief Display_fill_circleを実行する。
         * @param receiver 検査済み引数。recordは値コピー、deviceは不透明ハンドル。
         * @param arg_center_x 検査済み引数。recordは値コピー、deviceは不透明ハンドル。
         * @param arg_center_y 検査済み引数。recordは値コピー、deviceは不透明ハンドル。
         * @param arg_radius 検査済み引数。recordは値コピー、deviceは不透明ハンドル。
         * @param arg_color 検査済み引数。recordは値コピー、deviceは不透明ハンドル。
         * @return None=成功、NativePending=再試行、その他は実行エラー。
         */
        virtual BytecodeVM::Error Display_fill_circle(uint32_t receiver, int32_t arg_center_x, int32_t arg_center_y, int32_t arg_radius, int32_t arg_color) = 0;
        /** @brief Display_displayを実行する。
         * @param receiver 検査済み引数。recordは値コピー、deviceは不透明ハンドル。
         * @return None=成功、NativePending=再試行、その他は実行エラー。
         */
        virtual BytecodeVM::Error Display_display(uint32_t receiver) = 0;
        /** @brief function_printを実行する。
         * @param arg_text 検査済み引数。recordは値コピー、deviceは不透明ハンドル。
         * @return None=成功、NativePending=再試行、その他は実行エラー。
         */
        virtual BytecodeVM::Error function_print(StringView arg_text) = 0;
    };

    /** @brief 生成したABIハッシュとID表をバックエンドへ接続する。@param backend 登録中保持する機器操作。@return VM登録表。 */
    BytecodeVM::NativeApi makeNativeApi(Backend &backend);
}
