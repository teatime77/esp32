# UroaProtocol

`uroa_protocol.h`の`uroa::Protocol`は、protocol v1と機器種別付きTelemetry version=3の読み書きを担当する。Arduino・BLE・Wi-Fiには依存しない。

- `command(data, length)`は10バイトをコピーする。不正な長さではversion=0を返す。
- `valid(command)`はversion、reserved、opcodeを検査する。CH・値・時間の検査は教材／機器側で行う。
- `urgentStop(data, length)`は従来互換の優先STOP判定。reserved等を検査する通常経路とは意図的に分かれている。
- `status(out, ...)`には12バイト以上の出力領域を渡す。
- `deviceTelemetry(out, capacity, flags, sequence, now, raw, imuCount)`はversion=3の8バイトヘッダーと、種別・加速度3軸・角速度3軸の13バイトレコードを`imuCount`個並べる。同じ物理IMUのサンプルを複数のIMUブロックへ共有する。戻り値は使用バイト数で、容量不足では0を返して出力を変更しない。IMU1個は21バイトになる。

角度は0.01度、時間はms。符号付きのIMU生データもビット列のままlittle endianへ変換する。シーケンス番号の管理・送信先の判断・ACKは教材側の責任。

ヘッダーは`include/uroa_protocol.h`、実装は`src/uroa_protocol.cpp`。設定変更・I2C・通信を伴うメソッドの実装は`.cpp`に置く。

## 速度操作の拡張（0.3.0以降）

version=1の10byte指令へ`ARM_SPEED=8`（value=0・duration=0）と`SET_SPEED=9`（value=符号付き0.01%、−10000〜10000・duration=0）を追加する。負数はlittle endianの2の補数。`readSigned16()`で読み出す。既存の操作1〜7、UUID、サイズ、角度の単位は維持する。

12byte Statusのflags bit4（`FLAG_SPEED=0x10`）は指定CHの速度モードを示す。この場合current/targetは符号付き0.01%で読み、残り時間は0。角度モードでは従来どおり符号なし0.01度。コード7はモード不一致で、無効化してから別モードで有効化する。

旧ファームウェアは新操作をFORMATとして拒否する。Web側は速度操作を角度操作へ置き換えず、対応ファームウェアが必要であることを表示する。詳細は[Protocol仕様](../../app/diagram-control/docs/protocol.md)を参照する。
