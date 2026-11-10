# ビルドと検証

## Pythonのtimeを経過時間へ変更（2026-11-10）

起点・精度・再実行とVMの分担は[DeviceNative::function_timeの仕様コメント](../app/diagram-control/src/device_native.cpp)を参照する。APIのシグネチャは維持し、生成済みJSON・C++の`--check`は成功した。

リポジトリールートから`C:\Users\hamad\.platformio\penv\Scripts\platformio.exe run --project-dir app/diagram-control -j 2`を実行し、`platformio.ini`の`default_envs`に従う`supermini_esp32s3`だけでコンパイル・リンク・`firmware.bin`生成に成功した。静的RAMは59068/327680バイト、Flashは1063873/1310720バイト。PlatformIOの既存キャッシュ・ロックファイルへの書込み権限を使用した。テスト・Upload・実機での経過時間・再実行・sleep・デバッガー停止中の戻り値確認は行っていない。Web側は宣言コメントだけの変更で、今回の公開ビルドは行っていない。

## Pythonのtimeの初回実装（2026-11-10、経過時間への変更前）

UNIX時刻を返す初回実装の検証記録。ユーザー指定により既存のfloat32を維持し、device.pyiからJSON・C++を再生成した。現在の仕様は[DeviceNative::function_timeの仕様コメント](../app/diagram-control/src/device_native.cpp)を参照する。

リポジトリールートから`C:\Users\hamad\.platformio\penv\Scripts\platformio.exe run --project-dir app/diagram-control -e xiao_esp32s3 -j 2`を実行し、コンパイル・リンク・`firmware.bin`生成に成功した。静的RAMは63136/327680バイト、Flashは1106853/3342336バイト。PlatformIOの既存キャッシュ・ロックファイルへの書込み権限を使用した。uroaの生成物の`--check`と`npm.cmd run verify`も成功し、配信されたdevice-api.jsonのSHA-256は生成元と一致した。テスト・Upload・ブラウザー操作・時計の設定と実機の戻り値確認は行っていない。

この初回ビルド完了後、ユーザーから今後は`app/diagram-control/platformio.ini`の`default_envs`だけをビルド確認する指定を受けた。以降は`platformio.exe run --project-dir app/diagram-control -j 2`を使い、`-e`で環境を固定・追加しない。

## SSD1306の白文字の再描画（2026-11-06）

同じ位置へ数値を繰り返し描くと古い字形が重なるため、白文字の背景設定を修正した。文字セル・空白・短い文字列へ更新する場合の消去、および黒・反転色の扱いは[uroa_display.hのprint()/setTextColor()仕様コメント](../lib/display/include/uroa_display.h)を参照する。

リポジトリールートから`C:\Users\hamad\.platformio\penv\Scripts\platformio.exe run --project-dir app/diagram-control -e xiao_esp32s3 -j 2`を実行し、コンパイル・リンク・`firmware.bin`生成に成功した。直前のVMの既定精度変更を含む作業ツリーで、静的RAMは63136/327680バイト、Flashは1106613/3342336バイト。PlatformIOキャッシュへの書込み権限を使用した。テスト・Upload・SSD1306の実機表示確認は行っていない。

## Python浮動小数点数の既定精度（2026-11-06）

f文字列の既定精度を変更した。書式省略・明示指定・整数との分担は[bytecode_vm.cppのformatValue()仕様コメント](../app/diagram-control/src/bytecode_vm.cpp)を参照する。

現行のビルド案内に合わせ、リポジトリールートから`C:\Users\hamad\.platformio\penv\Scripts\platformio.exe run --project-dir app/diagram-control -e xiao_esp32s3 -j 2`を実行し、コンパイル・リンク・`firmware.bin`生成に成功した。静的RAMは63136/327680バイト、Flashは1106613/3342336バイト。PlatformIOキャッシュへの書込み権限を使用した。テスト・Upload・SSD1306とシリアルの実機表示確認は行っていない。

## Wi-Fiの結果Read応答のCORS重複（2026-11-06）

`controlValueHTTP()`と`respond()`によるCORSヘッダーの二重追加を修正した。成功・認証失敗・404・503の各経路の担当とOriginの寿命は[uroa_wifi.cppのCorsHeaders/controlValueHTTP()の仕様コメント](../lib/wifi/src/uroa_wifi.cpp)を参照する。対象は`/bytecode-result`と、同じ処理を共有する`/debug-result`・`/debug-event`・`/debug-data`。

ユーザー指定に従い、リポジトリールートから`C:\Users\hamad\.platformio\penv\Scripts\platformio.exe run --project-dir app/diagram-control -e xiao_esp32s3 -j 2`だけをビルドし、成功した。静的RAMは63136/327680バイト、Flashは1106597/3342336バイト。PlatformIOキャッシュへの書込み権限を使用した。テスト・Upload・ブラウザーからのPython Startと実Wi-Fi通信の再確認は行っていない。

## Wi-Fi設定情報取得時のBLEホストスタック消費（2026-11-06）

ユーザー報告の`nimble_host`スタックカナリー異常を、報告ELFハッシュ`55341021441d730e`と一致する手元のELFで解析した。`Configuration::onWrite()` → `WifiNetwork::configure()` → `jsonReply()` → ArduinoJson直列化 → String再確保の経路だった。認証ログは暗号化・MITM認証・ボンド成立を示しており、このクラッシュをペアリング不足として扱わない。

JSON作業領域・確保失敗・応答生成の修正は[uroa_wifi.cppのconfigure()/jsonReply()の仕様コメント](../lib/wifi/src/uroa_wifi.cpp)を参照する。ユーザー指定に従い、リポジトリールートから`C:\Users\hamad\.platformio\penv\Scripts\platformio.exe run --project-dir app/diagram-control -e xiao_esp32s3 -j 2`だけをビルドし、成功した。静的RAMは63136/327680バイト、Flashは1106561/3342336バイト。JSONのヒープ確保は静的RAM値に含まない。PlatformIOキャッシュへの書込み権限を使用した。テスト・Upload・Wi-Fi設定時の実機再確認は行っていない。

## BLE接続後のWi-Fi設定認証（2026-11-06）

認証開始と状態検査は[uroa_ble.cppの仕様コメント](../lib/ble/src/uroa_ble.cpp)、Not pairedの案内は[robot-wifi.ts](../../uroa/diagram/ts/ble-wifi/robot-wifi.ts)を参照する。

ユーザー指定に従い、リポジトリールートから`C:\Users\hamad\.platformio\penv\Scripts\platformio.exe run --project-dir app/diagram-control -e xiao_esp32s3 -j 2`でビルドし、成功した。静的RAMは63136/327680バイト、Flashは1105365/3342336バイト。既存PlatformIOキャッシュへの書込み権限を使用した。テスト・uroaのビルド・Upload・WindowsでのペアリングとWi-Fi接続の実機確認は行っていない。

## Pythonの再開・ステップ操作（2026-11-06）

VMの停止点設定・再開・フレーム識別・ネイティブ再試行は[bytecode_vm.cpp](../app/diagram-control/src/bytecode_vm.cpp)、停止IDの検証と要求受付は[bytecode_debugger.cpp](../app/diagram-control/src/bytecode_debugger.cpp)、通信形式の正本はuroaの[debug-protocol.ts](../../uroa/diagram/ts/ble-wifi/debug-protocol.ts)を参照する。

この会話の指定に合わせ、リポジトリールートから`C:\Users\hamad\.platformio\penv\Scripts\platformio.exe run --project-dir app/diagram-control -e esp32dev -j 2`だけを実行した。PlatformIOの既存キャッシュへの書込み権限を使用し、コンパイル・リンク・`firmware.bin`生成に成功した。静的RAMは62832/327680バイト、Flashは1207325/1310720バイト。ステップ用PC表やVM・Snapshotの動的確保は静的RAMに含まない。

uroaの変更ソースは`noEmit`の静的型診断0件を確認した。テストは作成・実行していない。XIAO・SuperMini・Web公開ビルド、Upload、ブラウザー操作、実BLE／Wi-Fi、各ステップ・ブレークポイント優先・再帰・ループ・ネイティブ待機・Stop競合・同一要求の再送・最大長の実機動作と空きRAMは未確認。

## 基板固有番号付きの表示名（2026-11-06）

表示名の生成とBLE／Wi-Fiへの反映は[device_name.cppの仕様コメント](../app/diagram-control/src/device_name.cpp)を参照する。今後のビルド確認はユーザー指定により`xiao_esp32s3`を対象とし、テストは行わない。

リポジトリールートから`C:\Users\hamad\.platformio\penv\Scripts\platformio.exe run --project-dir app/diagram-control -e xiao_esp32s3 -j 2`を実行し、コンパイル・リンク・`firmware.bin`生成に成功した。静的RAMは62848/327680バイト、Flashは1104073/3342336バイト。指定変更前に実行した`esp32dev`も成功した。PlatformIOキャッシュへの書込み権限を使用した。テスト・Upload・Windowsやブラウザーでの表示確認は行っていない。

## 共通接続APIの名称整理（2026-11-06）

uroaの共通接続窓口は[device-connection.ts](../../uroa/diagram/ts/ble-wifi/device-connection.ts)へ改名した。ESP32の仕様コメント・参照文書と共通プロトコルの表題、通信に依存しないPlotライブラリーの説明も対応させた。BLE固有のBleTransport・GATT・UUID・BLE設定処理は引き続き固有名で区別する。

ユーザー指定に従い、リポジトリールートから`C:\Users\hamad\.platformio\penv\Scripts\platformio.exe run --project-dir app/diagram-control -e esp32dev -j 2`だけをビルドし、成功した。既存PlatformIOキャッシュへの書込み権限を使用した。静的RAMは62536/327680バイト、Flashは1205585/1310720バイト。uroaは成果物を出力しない静的型確認で46ソースの診断0件を確認した。テストと公開ビルドは実行していない。

XIAO・SuperMiniのビルド、Upload、ブラウザーの表示・保存／復元、実BLE／Wi-Fi・実機動作は未確認。作業中のVM・デバッガ変更を保持し、上記のビルドには現在の作業ツリーを使用した。

## Pythonブレークポイントと変数表示（2026-11-06）

停止Snapshotの文字列直列化は[bytecode_vm.cpp](../app/diagram-control/src/bytecode_vm.cpp)、確保・配送は[bytecode_debugger.cpp](../app/diagram-control/src/bytecode_debugger.cpp)、通信形式の正本はuroaの[debug-protocol.ts](../../uroa/diagram/ts/ble-wifi/debug-protocol.ts)。画面との接続は[PythonBlock](../../uroa/diagram/ts/python/python-block.ts)、フレーム復元・値表示は[debugger-view.ts](../../uroa/diagram/ts/python/debugger-view.ts)を参照する。

ユーザー指定に従い、リポジトリールートで`C:\Users\hamad\.platformio\penv\Scripts\platformio.exe run --project-dir app/diagram-control -e esp32dev -j 2`だけをビルドした。初回の既存キャッシュの書込み制限は、同じコマンドにキャッシュ・ロックファイルへの権限を付けた再実行で解消した。コンパイル・リンク・`firmware.bin`生成は成功し、静的RAMは62536/327680バイト、Flashは1205585/1310720バイト。

テストは作成・実行していない。XIAO・SuperMiniのビルド、Upload、ブラウザー操作、実BLE／Wi-Fi、ブレークポイント停止・フレーム選択・構造体・文字列の実機表示、最大Snapshotの通信時間と空きRAMは未確認。静的RAMにはSnapshot・文字列・VM等の動的確保を含まない。uroa側は変更ソースの成果物を出力しない静的型確認と差分確認だけで、公開ビルドは実行していない。

## プログラム・VM・デバッグ通信のBLE／Wi-Fi共通化（2026-11-05）

受信元の共通識別子は[control_session.h](../app/diagram-control/include/control_session.h)、結果配送は[ControlTransport](../app/diagram-control/include/control_transport.h)、受信・保持は[BytecodeReceiver](../app/diagram-control/src/bytecode_receiver.cpp)、ブレークポイント・停止値は[BytecodeDebugger](../app/diagram-control/src/bytecode_debugger.cpp)、実行元の照合は[main.cpp](../app/diagram-control/src/main.cpp)を参照する。バイト形式・Wi-Fiの認証と独立したRead窓口の正本はuroaの[bytecode-transfer.ts](../../uroa/diagram/ts/ble-wifi/bytecode-transfer.ts)と[debug-protocol.ts](../../uroa/diagram/ts/ble-wifi/debug-protocol.ts)。

ユーザー指定に従い、テストは作成・実行せず、リポジトリールートから`C:\Users\hamad\.platformio\penv\Scripts\platformio.exe run --project-dir app/diagram-control -e esp32dev -j 2`だけをビルドした。既存PlatformIOキャッシュへの書込み権限を使用し、コンパイル・リンク・`firmware.bin`生成は成功した。静的RAMは62536/327680バイト、Flashは1205173/1310720バイト。uroaの`npm.cmd run verify`も成功した。

XIAO・SuperMiniのビルド、Upload、ブラウザー操作、実BLE／Wi-Fi、署名検証・切断・切替・取消・期限超過・最大長での通信時間と空きRAM、VM停止・機器出力の実動作は未確認。静的RAM値にはプログラム・Snapshot・VM等の動的確保は含まれない。

## 機器一覧のWi-Fi対応（2026-11-05）

共通登録APIは[control-connection.tsのsendDeviceInfo()](../../uroa/diagram/ts/ble-wifi/control-connection.ts)、Wi-Fiの認証・配送は[uroa_wifi.cppのdispatchControl()/controlHTTP()/socketEvent()](../lib/wifi/src/uroa_wifi.cpp)、機器一覧の検証・置換は[main.cppのreceiveDeviceInfo()/handleDeviceInfo()](../app/diagram-control/src/main.cpp)を参照する。

ユーザー指定に従い、テストは作成・実行せず、リポジトリールートから`C:\Users\hamad\.platformio\penv\Scripts\platformio.exe run --project-dir app/diagram-control -e esp32dev -j 2`だけでビルドした。既存PlatformIOキャッシュへの書込み権限を使用し、最終ソースのコンパイル・リンク・`firmware.bin`生成は成功した。静的RAMは62232/327680バイト、Flashは1202117/1310720バイト。uroaの`npm.cmd run verify`も成功した。

XIAO・SuperMiniのビルド、Upload、ブラウザー操作、実BLE／Wi-Fi、空一覧・最大長・不正形式・認証エラー・切断・世代変更・STREAM再開の実動作は未確認。静的RAM値にはVM等の動的確保は含まれない。

## 通常指令・通知の共通層抽出（2026-11-05）

通常通信の共通窓口は[control_transport.h](../app/diagram-control/include/control_transport.h)、Wi-Fiの通知APIは[uroa_wifi.h](../lib/wifi/include/uroa_wifi.h)、Web側は[control-connection.ts](../../uroa/diagram/ts/ble-wifi/control-connection.ts)を参照する。

ユーザー指定に従い、テストは作成・実行せず、リポジトリールートから`C:\Users\hamad\.platformio\penv\Scripts\platformio.exe run --project-dir app/diagram-control -e esp32dev -j 2`だけをビルドした。コンパイル・リンク・`firmware.bin`生成は成功し、静的RAMは62216/327680バイト、Flashは1201149/1310720バイト。初回はPlatformIOの既存ロックファイルへの書込み制限で停止し、キャッシュへの書込み権限を付けた再実行で完了した。Web側の`npm.cmd run verify`も成功した。

XIAO・SuperMiniのビルド、Upload、ブラウザー操作、実BLE／Wi-Fi、切替・切断・ACK期限超過・停止・センサー通知の実機動作は未確認。静的RAM値にはVM等の動的確保は含まれない。

## PythonデバッガのBLE通信（2026-11-05）

要求処理は[bytecode_debugger.cpp](../app/diagram-control/src/bytecode_debugger.cpp)、停止・値の直列化は[bytecode_vm.cpp](../app/diagram-control/src/bytecode_vm.cpp)、ワイヤー形式の正本とdiagram側の復号は[debug-protocol.ts](../../uroa/diagram/ts/ble-wifi/debug-protocol.ts)を参照する。

ユーザー指定に従い、テストは作成・実行せず、`C:\Users\hamad\.platformio\penv\Scripts\platformio.exe run --project-dir app/diagram-control -e esp32dev -j 2`でビルドする。初回は既存PlatformIOキャッシュの書込み制限で停止し、同じコマンドにキャッシュの書込み権限を付けて再実行した。実BLE、通知欠落時の回復、最大件数・Snapshotでの通信時間と空きRAM、VM停止と機器出力、Upload、XIAO・SuperMiniのビルドは未確認。静的RAMの報告値には一覧・Snapshot等の動的確保は含まれない。

esp32devのコンパイル・リンク・`firmware.bin`生成は成功した。静的RAMは62208/327680バイト、Flashは1200889/1310720バイト。uroaの`npm.cmd run verify`、両リポジトリーの`git -c core.whitespace=cr-at-eol diff --check`、更新文書のローカルリンク確認も成功した。

## Pythonのsleep（2026-11-05）

待機APIとVMの接続は[function_sleepの仕様コメント](../app/diagram-control/src/device_native.cpp)を参照する。uroa側の[device.pyi](../../uroa/diagram/python/device.pyi)からJSON・C++スタブ・ABI指紋を再生成する。uroaルートで`python diagram/python/generate-device-api.py --check`を実行し、生成物が一致することを静的確認する。

従来のユーザー指定に従いテストは作成・実行せず、リポジトリールートの`platformio.exe run --project-dir app/diagram-control -e esp32dev -j 2`だけでコンパイル・リンク・バイナリー生成を確認する。実機へのUpload、待機時間・再開位置・Stop・切断・millisのラップ時の実動作、XIAO・SuperMiniのビルドは未確認。

生成物の`--check`とesp32devのビルドは成功した。静的RAMは61968/327680バイト、Flashは1196569/1310720バイト。VM等の動的確保は静的RAM値に含まれない。既存PlatformIOキャッシュへの書込み権限を使い、実機への書込みは行っていない。uroaの`npm.cmd run verify`と両リポジトリーの`git -c core.whitespace=cr-at-eol diff --check`も成功した。

## PythonBlockのVM開始・停止（2026-11-05）

uroaのPythonBlockに合わせてProtocolとdiagram-controlを変更した。開始・停止と受信完了の分担は[main.cppの仕様コメント](../app/diagram-control/src/main.cpp)、Web側は[python-block.ts](../../uroa/diagram/ts/python/python-block.ts)を参照する。

リポジトリールートで`platformio.exe run --project-dir app/diagram-control -e esp32dev -j 2`を実行し、コンパイル・リンク・`firmware.bin`生成に成功した。初回は既存PlatformIOキャッシュの書込み制限で停止し、キャッシュへの権限を付けて再実行した。静的RAMは61952/327680バイト、Flashは1195937/1310720バイト。動的確保量は静的RAM値に含まれない。従来のユーザー指定に従いテストは作成・実行しない。XIAO・SuperMiniのビルド、Upload、実BLE、VMと機器の実機動作は未確認。

## SSD1306共有ライブラリー（2026-11-05）

`lib/display`を追加し、diagram-controlのPython `Display`から共通ドライバーを使う構成へ変更した。仕様は[uroa_display.h](../lib/display/include/uroa_display.h)、VMとの接続は[device_native.cpp](../app/diagram-control/src/device_native.cpp)、使用例は[README](../lib/display/README.md)を参照する。

リポジトリールートで`platformio.exe run --project-dir app/diagram-control -e esp32dev -j 2`を実行する。ユーザー指定によりesp32devだけを対象とし、テストは作成・実行しない。PlatformIOの既存キャッシュとロックファイルへの書き込み権限を使用した。

コンパイル・リンク・`firmware.bin`生成に成功。静的RAMは61952/327680バイト、Flashは1195737/1310720バイト。表示バッファとVM等の動的確保は静的RAM値に含まれない。XIAO・SuperMiniのビルド、Upload、実I2C・SSD1306表示・VMの実機実行は未確認。

## Python機器スタブ（2026-11-05）

uroaルートの`python diagram/python/generate-device-api.py`でJSONとC++ヘッダー・実装を生成する。`--check`は3成果物の一致確認、`--json-only`は従来のJSONだけを生成する。生成と機器操作の仕様は[生成処理](../../uroa/diagram/python/device_api_cpp.py)と[DeviceNativeの各メソッド](../app/diagram-control/src/device_native.cpp)を参照する。

ユーザー指定により、`app/diagram-control`で`platformio.exe run -e esp32dev`だけを実行する。テストは作成・実行せず、XIAO・SuperMiniのビルド、Upload、実BLE・センサー・サーボ・表示器の実機確認は行わない。uroa側はPythonの生成処理と仕様コメントの変更で、TypeScriptの実行コードは変更しない。

最終ビルドは成功。静的RAMは61944/327680バイト、Flashは1194777/1310720バイト。生成プログラムの`--check`で3成果物の完全一致を確認した。両リポジトリーの`git diff --check`も成功。VM・受信バッファ・SSD1306表示バッファ等の動的確保は静的RAM値に含まれない。npmビルドとテストは行っていない。

## Python VMの構造体・ネイティブ呼出し（2026-11-04）

[VM](../app/diagram-control/src/bytecode_vm.cpp)へversion=3の値スロット・呼出し規約とネイティブ登録APIを実装した。実行範囲・所有権・スタブの接続方法と未実装部分は同ファイル先頭コメントを参照する。

`app/diagram-control`から`platformio.exe run -e supermini_esp32s3 -e esp32dev -e xiao_esp32s3`でビルドする。ユーザー指定によりテストは作成・実行しない。Upload・実BLE・VMの実機実行は行わない。機器スタブ本体と生成登録表は今回の対象外。

最終ソースで3環境ともビルド成功。静的RAM/FlashはDevKitが61448/1174785バイト、XIAOが61784/1071981バイト、SuperMiniが57708/1028993バイト。動的スタック・大域変数・文字列・受信バッファの確保量は静的RAMに含まれない。差分の空白検査も成功した。uroa側は仕様コメントだけの変更で、npmビルドは実行していない。

## Pythonバイナリversion=3のBLE受信（2026-11-04）

diagram側の生成・送信に合わせ、[受信器](../app/diagram-control/src/bytecode_receiver.cpp)の形式検証を更新した。VM本体は変更していない。対応範囲と実行前に必要な照合は受信器先頭コメントを参照する。

`app/diagram-control`から`platformio.exe run -e supermini_esp32s3 -e esp32dev -e xiao_esp32s3`でビルドする。ユーザー指定によりテストは作成・実行せず、Upload・実BLE通信・実機確認も行わない。uroa側のビルドは別途実施する。

3環境ともビルド成功。初回のユーザーキャッシュへの書込み制限は、権限を付けた再実行で解消した。uroaの`npm.cmd run verify`も成功した。VM本体とmain.cppの実行処理は変更していない。受信ACK・ABI互換性・VM実行をそれぞれ区別し、ビルドだけで動作確認済みとは扱わない。

## PythonサブセットVM（2026-11-03）

BytecodeVMを追加し、受信器の正常COMMIT後にloop()から開始する。24命令、int32・float32・None、四則・比較・分岐・関数フレーム・再帰を実装した。未初期化、型、スタック、深さ、0除算、整数オーバーフロー、非有限floatを実行時に検査する。1周128命令までとし、STOP・切断・通信期限切れで停止する。VMの結果・上限・状態は[仕様](../app/diagram-control/src/bytecode_vm.cpp)を参照する。

ユーザー指定に従いDevKitの`esp32dev`だけを`platformio.exe run --project-dir app/diagram-control -e esp32dev -j 1`でビルドし、最終版が成功した。静的RAMは60608/327680バイト、Flashは1168593/1310720バイト。VMの動的スタック・大域変数・受信バッファは静的RAM使用量に含まれない。テスト、他ボードのビルド、Upload、実BLE・VMの実機実行は行っていない。

## 利用プロジェクトからの参照

ルートは共通ライブラリーと教材アプリをまとめる。PlatformIOの設定と実行コードは各`app/<教材>/`に置く。各アプリは必要な共通ライブラリーを参照する。

`app/<教材>/`からの相対参照例:

```ini
lib_deps =
    symlink://../../lib/protocol
    symlink://../../lib/imu
    symlink://../../lib/ble
    symlink://../../lib/plot
```

別リポジトリーの利用プロジェクトからは、そのプロジェクトを起点に相対パスを指定するか、`symlink://D:/usr/prj/esp32/lib/imu`などを指定する。`symlink://`は共通ソースを編集した結果を次のビルドに反映する。外部依存は各ライブラリーの`library.json`から導入する。

利用側ではVS Code + PlatformIOを使用し、対象環境を明示してビルド・書き込みする。書き込みはユーザーが指示した場合のみ行う。

```powershell
pio run --project-dir <利用プロジェクト> -e <対象環境>
pio run --project-dir <利用プロジェクト> -e <対象環境> -t upload
```

## 検証記録

2026-10-28: 6ライブラリーを参照する一時プロジェクトで、各ライブラリーのコンパイルと検証用ファームウェアのリンクを確認した。ビルド用の一時プロジェクトはこのリポジトリーへ含めず、倒立アプリも作成していない。

| 対象 | PlatformIO環境 | 結果 |
|---|---|---|
| ESP32 DevKit | `esp32dev` / `espressif32@6.10.0` / Arduino | 成功 |
| XIAO ESP32-S3 | `seeed_xiao_esp32s3` / `espressif32@6.10.0` / Arduino | 成功 |
| 配置・API | 6ライブラリーのヘッダーと実装、JSON、単純アクセサの除去、公開atomicの型 | 静的確認済み |
| 文書・書式 | リポジトリー内の文書リンク、clang-format | 確認済み |

検証用コードは公開状態の参照、`std::atomic`の型、コンストラクターの外部定義、Protocol・Plotなどを参照する。各ライブラリーの全操作や通信シーケンスを実行した試験ではない。ProtocolとPlotの実装はコメント・空白を除いて元のコードと一致することも確認した。

検証環境では、既存PlatformIOキャッシュへの書き込み権限が必要だった。また、XIAO側の長いSDK参照パスによってコンパイラーの子プロセス起動が失敗したため、検証用に既存パッケージディレクトリーへの短いジャンクションを作り、`PLATFORMIO_PACKAGES_DIR`をそのパスに指定して再実行した。並列数は2とした。ライブラリーのコードや依存バージョンを変更せずに解消し、リポジトリーに環境固有のパッケージパスは固定していない。

コンパイルとリンクの成功は、実機への書き込み、BLE/Wi-Fiの実通信、I2C機器・サーボの動作確認を意味しない。


## diagram-control（2026-10-28）

アプリの[操作・配線](../app/diagram-control/README.md)と[通信形式](../app/diagram-control/docs/protocol.md)を参照する。リポジトリールートから次を実行する。

```powershell
pio run --project-dir app/diagram-control -e esp32dev
pio run --project-dir app/diagram-control -e xiao_esp32s3
```

作成したアプリと共通ライブラリーを一時作業フォルダーに同じ相対配置で置き、PlatformIOでコンパイル、リンク、サイズ確認、`firmware.bin`生成を確認した。PlatformIOは`espressif32@6.10.0`、Arduinoフレームワークを使用した。

| 環境 | 結果 | RAM | Flash |
|---|---|---|---|
| `esp32dev` | SUCCESS | 58,968 / 327,680 bytes | 1,154,573 / 1,310,720 bytes |
| `xiao_esp32s3` | SUCCESS | 59,304 / 327,680 bytes | 1,054,245 / 3,342,336 bytes |

検証環境では、既存PlatformIOキャッシュへの書き込み権限と、前述の短いパッケージ参照パスを使用した。並列数は4。検証用のパスをリポジトリーの設定へ固定せず、生成物も移行先へ含めていない。

以下は静的に確認した。

- diagramの6種のUUIDおよび教材用共通設定との一致。
- 2基板の選択、共通ライブラリーの相対参照、`.h`と`.cpp`の配置、新しい公開状態APIの使用。
- コード書式、文書リンク、共通ライブラリーのコードを変更していないこと。
- 指令はloop()で処理し、STOPを優先すること、現在の通信経路・接続世代を照合すること、サーボ自動ARMを行わないことをソースで確認した。

起動、サーボの移動・停止、IMU通知、BLE/Wi-Fi切り替え、NVSの引継ぎ、カメラJPEG、Android・ブラウザーとの実通信は未確認。ハードウェアを模擬して実行する試験も行っていない。今回、実機への書き込みは実施していない。


## 角度／速度モード（2026-10-28）

Servo・Protocolを0.3.0へ更新し、diagram-controlでmoveAngle()とsetSpeed()を使い分ける。角度用opcode 1/2を維持して速度用opcode 8/9を追加し、Status bit4とcode=7でモードを区別する。旧C++のarm()/move()は互換窓口として残す。

| 環境 | 結果 | RAM | Flash |
|---|---|---|---|
| esp32dev | SUCCESS | 59,192 / 327,680 bytes | 1,155,997 / 1,310,720 bytes |
| xiao_esp32s3 | SUCCESS | 59,528 / 327,680 bytes | 1,055,593 / 3,342,336 bytes |

同じ相対配置の検証用プロジェクトでespressif32@6.10.0を使い、コンパイル・リンク・バイナリー生成を確認した。既存ビルドキャッシュと短いパッケージ参照を利用した。コード書式・ヘッダーの分離とプロトコルの定義を静的確認した。

uroa側はnpm.cmd run verifyと模擬BLEのサーボテスト12件が成功。新しい速度操作・切り替え・取消・旧JSONと既存角度操作を確認した。実機のI2Cを模擬実行した試験、書き込み、実BLE/Wi-Fi・サーボの中立/方向/実速度/停止は未確認。

## diagram-controlのSuperMini対応（2026-10-28）

`supermini_esp32s3`環境を追加した。SDA=GPIO8・SCL=GPIO9、Flash 4MB・DIO・USB CDC、PSRAM未使用。既定環境のXIAOは変更せず、SuperMiniを使う場合は環境を明示する。配線とBuild・Upload・Monitor手順は[アプリREADME](../app/diagram-control/README.md)を参照する。

同じ相対配置の一時作業フォルダーでビルドし、3環境ともコンパイル・リンク・サイズ確認・バイナリー生成に成功した。

| 環境 | 結果 | 時間 | RAM | Flash |
|---|---|---|---|---|
| esp32dev | SUCCESS | 220.13秒 | 59,192 / 327,680 bytes | 1,155,997 / 1,310,720 bytes |
| xiao_esp32s3 | SUCCESS | 222.83秒 | 59,528 / 327,680 bytes | 1,055,593 / 3,342,336 bytes |
| supermini_esp32s3 | SUCCESS | 238.67秒 | 55,452 / 327,680 bytes | 1,012,721 / 1,310,720 bytes |

SuperMiniは4MB Flash用のパーティションでアプリ領域1,310,720 bytesに収まった。3基板のSDA/SCL分岐も静的アサートを使ってコンパイル確認した。今回、外部依存を含む3環境の再ビルドで合計11分21秒かかった。コード書式・文書リンクを確認し、共通ライブラリーとアプリの制御処理は変更していない。

実機への書き込み・起動・USB CDC・BLE/Wi-Fi・サーボ・IMUは未確認。ビルド成功から実機の動作確認済みとは扱わない。


## カレントフォルダーからビルド・Upload

[bin/build.ps1](../bin/build.ps1)は、実行時のカレントフォルダーの`platformio.ini`を使う。`pio run --target upload`を1回実行し、ビルドに成功すると書き込む。失敗時はPlatformIOの終了コードを返す。

```powershell
cd D:\usr\prj\esp32\app\diagram-control
D:\usr\prj\esp32\bin\build.ps1
# 環境とポートを明示する場合
D:\usr\prj\esp32\bin\build.ps1 -Environment supermini_esp32s3 -Port COM5
```

引数を省略すると環境は`default_envs`に従う。`default_envs`がない場合はPlatformIOの標準動作で全環境が対象になる。ポートは`upload_port`などのPlatformIO設定に従い、未指定なら自動検出する。COM5は例であり、実際の接続先に合わせる。このスクリプトはMonitorを起動しない。

PATH上の`pio`または`platformio`を探し、見つからなければ`%USERPROFILE%\.platformio\penv\Scripts\platformio.exe`を使う。


## カレントフォルダーからシリアルモニターを開く

[bin/serial.ps1](../bin/serial.ps1)は、カレントフォルダーで`pio device monitor --project-dir <カレントフォルダー>`を実行する。

```powershell
cd D:\usr\prj\esp32\app\diagram-control
D:\usr\prj\esp32\bin\serial.ps1
# 必要に応じて環境とポートを指定する
D:\usr\prj\esp32\bin\serial.ps1 -Environment supermini_esp32s3 -Port COM5
```

ポート引数を省略すると`monitor_port`の設定を使い、その設定もなければPlatformIOが自動検出する。複数のボードを接続して目的のポートが選ばれない場合は`-Port`を指定する。通信速度、DTR、RTSなどは選択された環境の`platformio.ini`設定を使う。環境を省略した場合もPlatformIOの既定環境選択に従う。

終了は`Ctrl+C`。Uploadする前にモニターを終了してポートを解放する。PlatformIOの探索方法は`build.ps1`と共通で、実行ファイルの配置場所がPATHになくても標準の仮想環境から起動できる。

参照: [PlatformIOのシリアルモニター](https://docs.platformio.org/en/latest/core/userguide/device/cmd_monitor.html)。

## 超音波距離の混在Telemetry（2026-11-01）

DeviceInfo順にIMU（13バイト）と距離（種別2・mm単位uint16・3バイト）を連結する送信処理を追加した。未測定・失敗は0xFFFF。距離だけのSTREAMではIMUを要求しない。HC-SR04の同期測定は専用タスクで行い、各処理後に既定100ms待機して、要求世代付きの最新1件キューでloop()へ結果を渡す。diagram側は変更していない。

リポジトリールートから`pio run --project-dir app/diagram-control -e esp32dev -j 2`でコンパイル・リンク・firmware.bin生成を確認した。実行には既存PlatformIOキャッシュとリポジトリーへの書き込み権限を使用した。

| 対象 | 結果 |
|---|---|
| `esp32dev` / `board = esp32doit-devkit-v1` / `espressif32@6.10.0` | SUCCESS、197.42秒 |
| 静的RAM | 59,592 / 327,680 bytes |
| アプリFlash | 1,161,253 / 1,310,720 bytes |
| ソース書式・差分・Protocol JSON・変更文書のローカルリンク | 確認済み |

ユーザー指定によりテストは作成・実行していない。Upload、GPIO配線、実距離測定、測定タスクの実行タイミング、STOPの応答時間、BLE/Wi-Fiの実通信、合意MTU、diagramでの受信は未確認。今回XIAO・SuperMini向けの再ビルドは実施していない。ビルド成功はこれらの実動作を示すものではない。
