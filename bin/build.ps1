<#
.SYNOPSIS
カレントフォルダーのPlatformIOプロジェクトをビルドして書き込む。
.DESCRIPTION
引数を省略するとplatformio.iniの既定環境とポート設定を使用する。
uploadターゲットはビルド成功後に書き込みを行う。終了コードを呼出元へ返す。
.PARAMETER Environment
PlatformIOの環境名。省略時はdefault_envsに従う。
.PARAMETER Port
書き込み先のポート。省略時はPlatformIOの設定または自動検出に従う。
.EXAMPLE
D:\usr\prj\esp32\bin\build.ps1
.EXAMPLE
D:\usr\prj\esp32\bin\build.ps1 -Environment supermini_esp32s3 -Port COM5
#>
[CmdletBinding()]
param(
    [ValidateNotNullOrEmpty()]
    [string]$Environment,

    [ValidateNotNullOrEmpty()]
    [string]$Port
)

$ErrorActionPreference = 'Stop'

try {
    $projectDirectory = (Get-Location).ProviderPath
    if (-not (Test-Path -LiteralPath (Join-Path $projectDirectory 'platformio.ini') -PathType Leaf)) {
        throw 'カレントフォルダーにplatformio.iniがありません。アプリのフォルダーへ移動してください。'
    }

    # 通常のPATH、続いてVS Code拡張が使用する標準の仮想環境を探す。
    $platformioCommand = Get-Command pio, platformio -CommandType Application -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($null -ne $platformioCommand) {
        $platformioPath = $platformioCommand.Source
    }
    else {
        $platformioPath = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\platformio.exe'
        if (-not (Test-Path -LiteralPath $platformioPath -PathType Leaf)) {
            throw 'PlatformIOが見つかりません。VS CodeのPlatformIO拡張を導入するか、pioをPATHへ追加してください。'
        }
    }

    $platformioArguments = @('run', '--project-dir', $projectDirectory, '--target', 'upload')
    if ($PSBoundParameters.ContainsKey('Environment')) {
        $platformioArguments += @('--environment', $Environment)
    }
    if ($PSBoundParameters.ContainsKey('Port')) {
        $platformioArguments += @('--upload-port', $Port)
    }

    Write-Host "ビルド・Upload: $projectDirectory"
    & $platformioPath @platformioArguments
    exit $LASTEXITCODE
}
catch {
    [Console]::Error.WriteLine($_.Exception.Message)
    exit 1
}
