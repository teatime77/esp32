/**
 * @file bytecode_format.h
 * @brief 現行バイトコードの固定配置と命令長の共通定義。
 * @details wire形式の正本はuroa/diagram/ts/python/bytecode.tsのencodeBinary。
 * bytecode_receiver.cppは命令・PC・UTF-8を検証し、bytecode_vm.cppは値と実行上限を検査する。
 * bytecode_debugger.cppも同じ配置・命令長を使う。版1/2は受理しない。
 */
#pragma once
#include <stddef.h>
#include <stdint.h>

/**
 * @brief バイナリversion=3のレイアウト。転送・デバッグ通信の版番号とは独立する。
 */
namespace BytecodeFormat {
    constexpr uint16_t Version = 3; ///< 受理する唯一のバイトコード版。
    constexpr size_t HeaderLength = 56; ///< ABI指紋を含むヘッダーのバイト数。
    constexpr size_t FunctionRecordLength = 12; ///< 関数レコードのバイト数。
    constexpr size_t StringRecordLength = 8; ///< 文字列レコードのバイト数。
    constexpr size_t MaxLength = 128 * 1024; ///< イメージ全体のバイト上限。

    /**
     * @brief 長さと固定ヘッダーを検証した借用ビュー。本体・命令の内容検証は呼出し側が行う。
     */
    struct Image {
        uint16_t globalCount = 0; ///< 大域値のスロット数。
        uint16_t functionCount = 0; ///< 関数レコード数。
        uint16_t stringCount = 0; ///< UTF-8定数のレコード数。
        uint32_t codeLength = 0; ///< 命令列のバイト長。
        uint32_t stringDataLength = 0; ///< UTF-8データのバイト長。
        const uint8_t *functions = nullptr; ///< 借用する関数表の先頭。
        const uint8_t *code = nullptr; ///< 借用する命令列の先頭。
        const uint8_t *strings = nullptr; ///< 借用する文字列表の先頭。
        const uint8_t *stringData = nullptr; ///< 借用するUTF-8データの先頭。
        const uint8_t *abiHash = nullptr; ///< ヘッダー内のSHA-256生32バイト。
    };

    /**
     * @brief version=3の固定ヘッダーと全領域の長さを検証し、成功時だけビューを設定する。
     * @param data イメージの先頭。ビューの使用中は有効に保持する。
     * @param length イメージ全長。
     * @param image 成功時の借用ビュー。失敗時は変更しない。
     * @return 識別子・版・規約・予約値・領域長が正しければtrue。
     */
    bool parseImage(const uint8_t *data, size_t length, Image &image);

    /**
     * @brief bit7を含まないopcodeの命令長を返す。フラグを持つ呼出し側は先に除去する。
     * @param opcode 命令番号。
     * @return 1/3/5バイト。不正なopcodeは0。
     */
    uint32_t instructionLength(uint8_t opcode);
}
