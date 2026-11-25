/**
 * @file numpy_native.cpp
 * @brief 数値NumPyサブセットの生成・添字・属性を同期実行する。
 * @details device.pyiと生成ヘッダーが関数ID・引数番号を共有し、semantic.tsはdtypeとrankを推論する。
 * bytecode.tsのCALL_NUMPY規約をVMが取り出し、本ファイルへ渡す。本体の確保・検証はVM、
 * 非移動GCはbytecode_heap.cpp、停止データの形式はdebug-protocol.tsを正本とする。
 */
#include "numpy_native.h"
#include "device_api.generated.h"
#include <cmath>
#include <cstring>

namespace {
    constexpr uint8_t DefaultDtype = 1; ///< 省略またはNoneのdtypeはfloat32。コンパイラーは値を補わない。
}

/**
 * @copydoc numpy_native::create
 * @details 省略またはNoneのdtypeはfloat32とする静的契約をsemantic.tsも使用する。
 * 既定引数の値はC++で設定する。動的dtypeは扱わず、明示dtypeはnp.float32・np.uint8・object・np.object_・None。
 * 既定orderはC。明示指定はCまたはNoneだけ、device/likeはNoneだけを許す。
 * emptyも当面ゼロ初期化する。未初期化の浮動小数点を公開せず、zerosと同じ安全な領域を使う。
 * 本体はVMの非移動GCヒープへ確保し、代入・引数・戻り値・メンバーは参照共有する。
 * 1～4次元・C順の連続領域。shapeの整数タプルは呼出し時だけ扱う。汎用タプル・リスト・
 * 部分配列・スライス・ビュー・broadcast・配列全体の演算は実装しない。
 * object dtypeはemptyだけに対応し、全要素をNoneにする。数値配列とobject配列も要素に格納できるが、
 * 要素型は単一クラスの参照＋Noneへ統一する。型表の要素タグ・クラスIDを格納・読出し時に照合する。
 * object配列のitemsizeはVMのValue幅。共有・循環を許し、Noneの属性・メソッド・配列操作はNoneAccessとする。
 */
BytecodeVM::Error numpy_native::create(BytecodeVM &vm, uint16_t function, uint16_t classId,
    const BytecodeVM::Value *shape, uint8_t rank, const Option *options, size_t count, BytecodeVM::Value &result) {
    using Error = BytecodeVM::Error;
    using Type = BytecodeVM::Type;
    using Function = device_api::numpy::Function;
    using Zeros = device_api::numpy::ZerosOption;
    using Empty = device_api::numpy::EmptyOption;
    const bool zeros = function == uint16_t(Function::zeros);
    if ((!zeros && function != uint16_t(Function::empty)) || count > 4 || rank < 1 || rank > 4) {
        return Error::ArrayOption;
    }
    const uint16_t dtypeId = zeros ? uint16_t(Zeros::dtype) : uint16_t(Empty::dtype);
    const uint16_t orderId = zeros ? uint16_t(Zeros::order) : uint16_t(Empty::order);
    const uint16_t deviceId = zeros ? uint16_t(Zeros::device) : uint16_t(Empty::device);
    const uint16_t likeId = zeros ? uint16_t(Zeros::like) : uint16_t(Empty::like);
    uint8_t dtype = DefaultDtype;
    for (size_t index = 0; index < count; ++index) {
        const Option &option = options[index];
        for (size_t previous = 0; previous < index; ++previous) {
            if (options[previous].id == option.id) {
                return Error::ArrayOption;
            }
        }
        if (option.id == dtypeId) {
            if (option.value.type != Type::Integer || option.value.integer < 0 || option.value.integer > 3) {
                return Error::ArrayOption;
            }
            dtype = option.value.integer == 0 ? DefaultDtype : uint8_t(option.value.integer);
        }
        else if (option.id == orderId) {
            if (option.value.type == Type::None) {
                continue;
            }
            const char *text = nullptr;
            size_t length = 0;
            if (!vm.stringBytes(option.value, text, length) || length != 1 || text[0] != 'C') {
                return Error::ArrayOption;
            }
        }
        else if (option.id == deviceId || option.id == likeId) {
            if (option.value.type != Type::None) {
                return Error::ArrayOption;
            }
        }
        else {
            return Error::ArrayOption;
        }
    }
    if (dtype == 3 && zeros) {
        return Error::ArrayOption;
    }
    uint32_t dimensions[4]{};
    for (uint8_t axis = 0; axis < rank; ++axis) {
        if (shape[axis].type != Type::Integer || shape[axis].integer < 0) {
            return Error::ArrayShape;
        }
        dimensions[axis] = uint32_t(shape[axis].integer);
    }
    return vm.allocateArray(classId, dtype, dimensions, rank, result) ? Error::None : vm.error;
}

/**
 * @copydoc numpy_native::item
 * @details 各負の添字はその軸長を一度足して補正する。補正後も範囲外ならArrayIndex。
 * float32は有限な数値だけ格納する。uint8は0～255の有限な数値を0方向へ切り捨て、範囲外は拒否する。
 * objectは参照を共有し、代入・上書きの際に確保・解放・GCを行わない。
 */
BytecodeVM::Error numpy_native::item(BytecodeVM &vm, const BytecodeVM::Value &receiver,
    const BytecodeVM::Value *indices, uint8_t rank, bool write, BytecodeVM::Value &value) {
    using Error = BytecodeVM::Error;
    using Type = BytecodeVM::Type;
    const BytecodeVM::Array *array = nullptr;
    if (!vm.arrayData(receiver, array)) {
        return receiver.type == Type::None ? Error::NoneAccess : Error::Type;
    }
    if (rank != array->rank) {
        return Error::ArrayIndex;
    }
    uint32_t offset = 0;
    for (uint8_t axis = 0; axis < rank; ++axis) {
        if (indices[axis].type != Type::Integer) {
            return Error::ArrayIndex;
        }
        int64_t index = indices[axis].integer;
        if (index < 0) {
            index += array->shape[axis];
        }
        if (index < 0 || uint64_t(index) >= array->shape[axis]) {
            return Error::ArrayIndex;
        }
        offset = offset * array->shape[axis] + uint32_t(index);
    }
    if (array->dtype == 3) {
        auto *values = static_cast<BytecodeVM::Value *>(receiver.reference->data);
        if (!vm.arrayElementValue(receiver, write ? value : values[offset])) {
            return Error::Type;
        }
        if (write) {
            values[offset] = value;
        }
        else {
            value = values[offset];
        }
        return Error::None;
    }
    auto *bytes = reinterpret_cast<uint8_t *>(const_cast<BytecodeVM::Array *>(array) + 1);
    if (write) {
        if (value.type != Type::Integer && value.type != Type::Float) {
            return Error::Type;
        }
        const double number = value.type == Type::Integer ? double(value.integer) : double(value.real);
        if (!std::isfinite(number)) {
            return Error::NonFinite;
        }
        if (array->dtype == 1) {
            const float real = float(number);
            if (!std::isfinite(real)) {
                return Error::NonFinite;
            }
            std::memcpy(bytes + size_t(offset) * sizeof(float), &real, sizeof real);
        }
        else {
            if (number < 0 || number > 255) {
                return Error::ArrayIndex;
            }
            bytes[offset] = uint8_t(number);
        }
    }
    else {
        value = BytecodeVM::Value{};
        if (array->dtype == 1) {
            value.type = Type::Float;
            std::memcpy(&value.real, bytes + size_t(offset) * sizeof(float), sizeof(float));
            if (!std::isfinite(value.real)) {
                return Error::NonFinite;
            }
        }
        else {
            value.type = Type::Integer;
            value.integer = bytes[offset];
        }
    }
    return Error::None;
}

/**
 * @copydoc numpy_native::attribute
 */
BytecodeVM::Error numpy_native::attribute(BytecodeVM &vm, const BytecodeVM::Value &receiver, uint16_t kind,
    const BytecodeVM::Value &axis, BytecodeVM::Value &result) {
    using Error = BytecodeVM::Error;
    const BytecodeVM::Array *array = nullptr;
    if (!vm.arrayData(receiver, array)) {
        return receiver.type == BytecodeVM::Type::None ? Error::NoneAccess : Error::Type;
    }
    uint32_t number = 0;
    switch (kind) {
        case 0:
            number = array->shape[0];
            break;
        case 1:
            number = array->elements;
            break;
        case 2:
            number = array->rank;
            break;
        case 3:
            number = array->dtype == 1 ? sizeof(float) : array->dtype == 2 ? 1 : sizeof(BytecodeVM::Value);
            break;
        case 4: {
            if (axis.type != BytecodeVM::Type::Integer) {
                return Error::ArrayIndex;
            }
            int64_t index = axis.integer;
            if (index < 0) {
                index += array->rank;
            }
            if (index < 0 || index >= array->rank) {
                return Error::ArrayIndex;
            }
            number = array->shape[index];
            break;
        }
        default:
            return Error::Operand;
    }
    result = BytecodeVM::Value{};
    result.type = BytecodeVM::Type::Integer;
    result.integer = int32_t(number);
    return Error::None;
}
