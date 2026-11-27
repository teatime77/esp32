/**
 * @file bytecode_format.h
 * @brief 現行バイトコードの命令番号・フラグ・固定配置・命令長の共通定義。
 * @details wire形式の正本はuroa/diagram/ts/python/bytecode.tsのencodeBinary。
 * bytecode_receiver.cppは命令・PC・UTF-8を検証し、bytecode_vm.cppは値と実行上限を検査する。
 * bytecode_debugger.cppも同じ配置・命令長を使う。版1/2は受理しない。
 */
#pragma once
#include <stddef.h>
#include <stdint.h>

/**
 * @brief バイナリversion=7のレイアウト。転送・デバッグ通信の版番号とは独立する。
 */
namespace BytecodeFormat {
    /**
     * @brief バイトコードの命令番号。VM・受信検証・命令長判定で共有する。
     * @details uroa/diagram/ts/python/bytecode.tsのBytecodeOpcodeと名前・値をそろえる。
     * wire値と直接比較できるuint8_t基底のenumとし、命令の識別にはOpcode::名前を使う。
     * 名前の導入で既存の命令番号・形式版・ABIを変更しない。
     */
    enum Opcode : uint8_t {
        LOAD_L = 0x01, ///< 局所変数を積む。
        LOAD_G = 0x02, ///< 大域変数を積む。
        PUSH_INT = 0x03, ///< int32定数を積む。
        PUSH_FLOAT = 0x04, ///< float32定数を積む。
        STORE_L = 0x05, ///< 局所変数へ格納する。
        STORE_G = 0x06, ///< 大域変数へ格納する。
        JUMP = 0x07, ///< 無条件で分岐する。
        JUMP_IF_FALSE = 0x08, ///< 偽なら分岐する。
        JUMP_IF_TRUE = 0x09, ///< 真なら分岐する。
        ADD = 0x0A, ///< 加算する。
        SUB = 0x0B, ///< 減算する。
        MUL = 0x0C, ///< 乗算する。
        DIV = 0x0D, ///< 除算する。
        EQ = 0x0E, ///< 等値を比較する。
        NE = 0x0F, ///< 不等を比較する。
        LT = 0x10, ///< より小さいか比較する。
        LE = 0x11, ///< 以下か比較する。
        GT = 0x12, ///< より大きいか比較する。
        GE = 0x13, ///< 以上か比較する。
        CALL = 0x14, ///< Python関数を呼ぶ。
        RET = 0x15, ///< 呼出し元へ戻る。
        PUSH_NONE = 0x16, ///< Noneを積む。
        POP = 0x17, ///< 先頭の値を捨てる。
        HALT = 0x18, ///< モジュールの実行を終了する。
        PUSH_STRING = 0x19, ///< 文字列定数の参照を積む。
        CALL_NATIVE = 0x1A, ///< ネイティブ関数を呼ぶ。
        TO_FLOAT = 0x1B, ///< 数値をfloat32へ変換する。
        FORMAT_VALUE = 0x1C, ///< 数値を文字列へ整形する。
        BUILD_STRING = 0x1D, ///< 文字列を連結する。
        NEW_OBJECT = 0x1E, ///< オブジェクト本体を確保する。
        GET_FIELD = 0x1F, ///< メンバー値を取得する。
        SET_FIELD = 0x20, ///< メンバー値を格納する。
        DUP = 0x21, ///< 先頭の値を複製する。
        CALL_NUMPY = 0x22, ///< NumPy関数を呼ぶ。
        GET_ITEM = 0x23, ///< 配列要素を取得する。
        SET_ITEM = 0x24, ///< 配列要素を格納する。
        ARRAY_ATTR = 0x25, ///< 配列の属性を取得する。
        CHECK_REF = 0x26, ///< 参照の型と有効性を検査する。
        GC_LOOP = 0x27, ///< メインループの安全点でGCを行う。
#ifdef __EMSCRIPTEN__
        CALL_KERNEL = 0x28 ///< ブラウザ版だけが扱う同期カーネル呼出し。
#endif
    };

    constexpr uint8_t OpcodeMask = 0x7F; ///< 命令バイトから命令番号だけを取り出すマスク。
    constexpr uint8_t BreakpointFlag = 0x80; ///< 命令先頭にデバッガーが設定する停止フラグ。
    constexpr uint16_t Version = 7; ///< GC本体型表を含む唯一のバイトコード版。
    constexpr size_t HeaderLength = 64; ///< ABI指紋と型表長を含むヘッダーのバイト数。
    constexpr size_t TypeRecordLength = 16; ///< 型ID・フィールド数・表位置・本体種別・rank・要素タグ・要素型・予約値。
    constexpr size_t FunctionRecordLength = 12; ///< 関数レコードのバイト数。
    constexpr size_t StringRecordLength = 8; ///< 文字列レコードのバイト数。
    constexpr size_t MaxLength = 128 * 1024; ///< イメージ全体のバイト上限。

    /**
     * @brief 長さと固定ヘッダーを検証した借用ビュー。本体・命令の内容検証は呼出し側が行う。
     */
    struct Image {
#ifdef __EMSCRIPTEN__
        const uint8_t *kernelHash = nullptr; ///< version 8だけが持つカーネル集合の指紋。
#endif
        uint16_t globalCount = 0; ///< 大域値のスロット数。
        uint16_t globalRootCount = 0; ///< 一時領域を除く大域変数の根の数。
        uint16_t functionCount = 0; ///< 関数レコード数。
        uint16_t stringCount = 0; ///< UTF-8定数のレコード数。
        uint16_t typeCount = 0; ///< GC本体型の数。
        uint32_t typeLength = 0; ///< 本体型表とフィールド表の合計バイト長。
        const uint8_t *types = nullptr; ///< 借用する本体型表。参照先IDも全件検証する。
        uint32_t codeLength = 0; ///< 命令列のバイト長。
        uint32_t stringDataLength = 0; ///< UTF-8データのバイト長。
        const uint8_t *functions = nullptr; ///< 借用する関数表の先頭。
        const uint8_t *code = nullptr; ///< 借用する命令列の先頭。
        const uint8_t *strings = nullptr; ///< 借用する文字列表の先頭。
        const uint8_t *stringData = nullptr; ///< 借用するUTF-8データの先頭。
        const uint8_t *abiHash = nullptr; ///< ヘッダー内のSHA-256生32バイト。
    };

    /**
     * @brief 固定ヘッダーと全領域の長さを検証し、成功時だけビューを設定する。
     * @details ESP32はversion=7、__EMSCRIPTEN__ではversion=8の96バイトヘッダーも扱う。
     * @param data イメージの先頭。ビューの使用中は有効に保持する。
     * @param length イメージ全長。
     * @param image 成功時の借用ビュー。失敗時は変更しない。
     * @return 識別子・版・規約・予約値・領域長が正しければtrue。
     */
    bool parseImage(const uint8_t *data, size_t length, Image &image);
    /**
     * @brief ソート済み本体型表から型IDを検索する。
     * @param types 検証済み型表。
     * @param count 型レコード数。
     * @param id 検索する非0 ID。
     * @return 型レコード。存在しなければnull。
     */
    const uint8_t *findType(const uint8_t *types, uint16_t count, uint16_t id);

    /**
     * @brief bit7を含まないopcodeの命令長を返す。フラグを持つ呼出し側は先に除去する。
     * @param opcode 命令番号。
     * @return 1/3/5バイト。不正なopcodeは0。
     */
    uint32_t instructionLength(uint8_t opcode);
}
