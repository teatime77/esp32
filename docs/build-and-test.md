# ビルドと検証

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
