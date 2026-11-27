# ブラウザ用Python VMのビルド

既存C++ VMをEmscriptenでWasm化し、Web側のPythonBlockから実行する。数値カーネル用のwcc資産も、固定ソースから開発環境で生成する。

ブラウザではPythonBlockの実行先で「ブラウザ」を選ぶ。Openから`numeric-kernel.py`を読み込み、StartでC生成・Wasmコンパイル・VM実行を開始する。`print("hello world")`のような文字列出力はPythonBlockへ改行付きで表示する。完了後は出力に続けて大域変数と配列の値を表示する。Stopは計算から戻った後にVMを停止する。

現在のブラウザ実行は通常のVM命令、自作NumPy、数値カーネルを対象とする。機器API呼出しは`NativeUnavailable`になる。行ブレークポイントの操作はESP32向けの既存機能であり、ブラウザでStartするときは解除する。

## 開発環境での資産生成

WindowsとPython、[公式Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html)を使う。Emscriptenは`4.0.23`、wccはコミット`659a1773f20499864a8c3fdc7cd25cb506092bc1`に固定する。

SDKのフォルダーで次を実行する。設定はそのSDKフォルダーに保持する。

```powershell
./emsdk.bat install 4.0.23
./emsdk.bat activate 4.0.23
```

wccの[固定ソース](https://github.com/tyfkda/xcc/tree/659a1773f20499864a8c3fdc7cd25cb506092bc1)を展開し、diagram-controlで次を実行する。パスは取得先に合わせる。

```powershell
python browser/build.py --emsdk C:/tools/emsdk --xcc C:/tools/xcc-659a1773f20499864a8c3fdc7cd25cb506092bc1 --output ../../../uroa/public/diagram/runtime/v1
```

`--xcc`を省略すると、既存のwcc資産を保ったままVMを再生成する。ビルド処理はEmscripten製の一時コンパイラーを用意し、wcc自身で標準ライブラリーと配布用`cc.wasm`を生成する。上流の`lib_list.json`と同じコンパイラー・ヘッダー・ライブラリーを`wccfiles.zip`へ格納する。

生成先にはVMのJS/Wasm、wcc資産、固定点とSHA-256を持つ`assets.json`、ライセンスを置く。Web側で固定依存をインストールし、告知と公開成果物を生成する。

```powershell
cd ../../../uroa
npm.cmd ci
node scripts/collect-browser-licenses.mjs
npm.cmd run verify
```

`verify`はWebの型・公開ビルドと、Worker・資産・固定版・ハッシュ・ライセンスの静的確認を行う。ブラウザでの実行確認は別に行う。

## 実装と仕様の参照先

- [host.cpp](host.cpp)：VMのC接続、引数・配列形式・共有の確認、同期呼出し。
- [browser-runtime.ts](../../../../uroa/diagram/ts/python/browser-runtime.ts)：PythonBlockの開始・停止とWorkerの所有。
- [browser-vm-worker.ts](../../../../uroa/diagram/ts/python/browser-vm-worker.ts)：VMと計算用Wasmの同居、配列コピーと実行。
- [browser-kernel-worker.ts](../../../../uroa/diagram/ts/python/browser-kernel-worker.ts)：wccとBinaryenの直列実行。
- [browser-abi.ts](../../../../uroa/diagram/ts/python/browser-abi.ts)：Workerの型と資産の照合。
- [bytecode.ts](../../../../uroa/diagram/ts/python/bytecode.ts)：ブラウザ専用version 8生成。

ブラウザ用命令とVMの接続は`__EMSCRIPTEN__`ビルドに限定する。ESP32の通常ビルドと既存の転送入口はversion 7を使う。
