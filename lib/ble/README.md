# UroaBLE

`publishStatus()`と`publishTelemetry()`は、各パケットを`notify(data, length)`へ渡して送信バッファーへコピーする。NimBLE-Arduino 2.3.6の無引数`notify()`はCharacteristicの更新を予約し、送信時にその時点の値を読むため、同じCharacteristicへ連続して書くと先の通知が後の値へ置き換わり得る。倒立教材のBalanceとIMU、連続するStatus応答を別々の内容で送るため、無引数の通知へ戻さない。`setValue()`はStatusのReadで最新値を返すために維持する。通知処理の成功は相手の受信・描画を保証しない。

`uroa_ble.h`の`uroa::BleTransport`はBLEサーバー、接続世代、指令の受信、Status/Telemetry通知と認証済みWi-Fi設定窓口を担当する。NimBLEは基板共通なので稼働するオブジェクトは1個とし、グローバルまたはstaticに置く。

`begin(config, callbacks)`へ名前・6種のUUID・ペアリングコードを渡す。コールバックの責任と公開状態の読み方は[設計](../../docs/architecture.md)を参照する。

| コールバック | 呼び出し元・責任 |
|---|---|
| `command(data, length, generation)` | BLEタスク。バッファは呼び出し中だけ有効。すぐコピーして指令キューへ送る |
| `session(connected, generation)` | BLEタスク。通常の接続・切断で呼ばれる。停止要求を予約する |
| `configure(json)` | 暗号化・MITM認証済みのBLEタスク。完成した設定JSONを渡し、結果JSONを受け取る。省略すると設定Characteristicを作らない |
| `started()` | `start()`の呼び出し元。サービス公開前の初期Status設定に使う |

`connected.load()`と`generation.load()`で古い指令を除外する。`publishStatus()`はREAD値を更新し、接続中は通知も行う。`publishTelemetry()`は接続中だけ通知し、送信APIが受理すればtrue、未接続・通知不可・送信失敗ならfalseを返す。戻り値は相手の受信を保証しない。倒立教材ではBalanceとIMUの送信成功数／試行数を別々に数え、途中から開いたモニターでも現在の状態を確認できる。

設定要求は先頭バイト0/1/2の分割JSON、384バイト未満。設定結果は512バイト以内とする。JSONの操作内容は解釈せず、通常の指令にもCH・角度の判断を行わない。

`stop()`はNimBLEをdeinitし、Characteristicへの参照を解除する。意図的な停止の切断では広告再開・sessionコールバックを行わない。`start()`は設定を再利用してサービス・広告を作り直す。Wi-Fi切り替え中の通知抑止と機器停止は教材側で組み合わせる。稼働中のオブジェクト破棄は扱わない。

`connected`と`generation`は公開の`std::atomic`であり、参照には`.load()`を使う。アプリから値を代入しても接続や制御権は変更できない。変更は通信イベントと操作メソッドに任せる。同期処理・接続世代の扱いは維持する。

ヘッダーは`include/uroa_ble.h`、実装は`src/uroa_ble.cpp`。設定変更・I2C・通信を伴うメソッドの実装は`.cpp`に置く。
