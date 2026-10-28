# UroaProtocol

`uroa_protocol.h`の`uroa::Protocol`は、protocol v1の読み書きを担当する。Arduino・BLE・Wi-Fiには依存しない。

- `command(data, length)`は10バイトをコピーする。不正な長さではversion=0を返す。
- `valid(command)`はversion、reserved、opcodeを検査する。CH・値・時間の検査は教材／機器側で行う。
- `urgentStop(data, length)`は従来互換の優先STOP判定。reserved等を検査する通常経路とは意図的に分かれている。
- `status(out, ...)`には12バイト以上、`telemetry(out, ..., raw)`には20バイト以上の出力領域を渡す。`raw`はMPU-6050の14バイト。

角度は0.01度、時間はms。符号付きのIMU生データもビット列のままlittle endianへ変換する。シーケンス番号の管理・送信先の判断・ACKは教材側の責任。

ヘッダーは`include/uroa_protocol.h`、実装は`src/uroa_protocol.cpp`。設定変更・I2C・通信を伴うメソッドの実装は`.cpp`に置く。
