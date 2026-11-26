/**
 * @file numpy_native.h
 * @brief 値スタックVMから使用する数値・object NumPyサブセット。実行契約はnumpy_native.cppを参照。
 */
#pragma once
#include "bytecode_vm.h"

/**
 * @brief GC本体を共有するndarrayの生成・要素操作。
 */
namespace numpy_native {
    /**
     * @brief 省略しなかった引数番号と値。文字列辞書や省略引数のスロット列は作らない。
     */
    struct Option {
        uint16_t id = 0; ///< device.pyiから生成した関数ごとのoptionId。
        BytecodeVM::Value value; ///< 同期実行中の値。dtypeは0=None・1=float32・2=uint8・3=object。
    };

    /**
     * @brief 共有のモジュール関数IDでzeros/emptyを実行する。
     * @param vm 確保先のVM。
     * @param function device_api::numpy::Functionの番号。
     * @param classId 静的な戻り値型ID。
     * @param shape rank個の軸長を表す整数値。
     * @param rank 次元数1～4。
     * @param options 指定された引数だけの一覧。
     * @param count 指定個数。最大4。
     * @param result 成功時の配列参照。
     * @return 成功時None、失敗時VMのエラー番号。
     */
    BytecodeVM::Error create(BytecodeVM &vm, uint16_t function, uint16_t classId, const BytecodeVM::Value *shape,
        uint8_t rank, const Option *options, size_t count, BytecodeVM::Value &result);
    /**
     * @brief 全軸の整数添字でスカラーを読書きする。
     * @param vm 本体を検査するVM。
     * @param receiver 配列参照。
     * @param indices rank個の整数添字。VMの値スタック上を同期処理中だけ参照する。
     * @param rank 指定した次元数。
     * @param write 書込む場合true。
     * @param value 書込み入力または読取り結果。
     * @return 成功時None、失敗時エラー番号。
     * @details primitiveはArray::getOffset()で位置を求め、型別に直接読み書きする。
     * 確保・GCを行わず、対象・添字・要素への参照をC++側へ持ち越さない。
     */
    BytecodeVM::Error item(BytecodeVM &vm, const BytecodeVM::Value &receiver, const BytecodeVM::Value *indices,
        uint8_t rank, bool write, BytecodeVM::Value &value);
    /**
     * @brief 配列のスカラー属性を読む。
     * @param vm 本体を検査するVM。
     * @param receiver 配列参照。
     * @param kind 0=len、1=size、2=ndim、3=itemsize、4=shapeの軸。
     * @param axis kind=4の場合の整数軸番号。
     * @param result 整数結果。
     * @return 成功時None、失敗時エラー番号。
     */
    BytecodeVM::Error attribute(BytecodeVM &vm, const BytecodeVM::Value &receiver, uint16_t kind,
        const BytecodeVM::Value &axis, BytecodeVM::Value &result);
}
