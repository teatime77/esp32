<#
.SYNOPSIS
カレントフォルダーのPlatformIOプロジェクトのシリアルモニターを開く。
.DESCRIPTION
platformio.iniの既定環境、通信速度、DTR/RTSなどの設定を使用する。
ポート設定も引数もない場合はPlatformIOの自動検出を使う。終了はCtrl+C。
.PARAMETER Environment
PlatformIOの環境名。省略時はPlatformIOが既定環境を選択する。
.PARAMETER Port
接続先のポート。省略時はmonitor_portの設定または自動検出に従う。
.EXAMPLE
D:\usr\prj\esp32\bin\serial.ps1
.EXAMPLE
D:\usr\prj\esp32\bin\serial.ps1 -Environment supermini_esp32s3 -Port COM5
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

    $platformioArguments = @('device', 'monitor', '--project-dir', $projectDirectory)
    if ($PSBoundParameters.ContainsKey('Environment')) {
        $platformioArguments += @('--environment', $Environment)
    }
    if ($PSBoundParameters.ContainsKey('Port')) {
        $platformioArguments += @('--port', $Port)
    }

    Write-Host "シリアルモニター: $projectDirectory (終了: Ctrl+C)"
    & $platformioPath @platformioArguments
    exit $LASTEXITCODE
}
catch {
    [Console]::Error.WriteLine($_.Exception.Message)
    exit 1
}
