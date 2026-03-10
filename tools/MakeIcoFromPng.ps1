param(
    [Parameter(Mandatory = $true)]
    [string]$PngPath,
    [Parameter(Mandatory = $true)]
    [string]$IcoPath
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $PngPath)) {
    throw "PNG not found: $PngPath"
}

$pngBytes = [System.IO.File]::ReadAllBytes($PngPath)
if ($pngBytes.Length -lt 8) {
    throw "Invalid PNG file: $PngPath"
}

$pngSignature = 0x89,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A
for ($i = 0; $i -lt 8; $i++) {
    if ($pngBytes[$i] -ne $pngSignature[$i]) {
        throw "File is not a valid PNG: $PngPath"
    }
}

$dir = Split-Path -Parent $IcoPath
if ($dir -and (-not (Test-Path -LiteralPath $dir))) {
    [System.IO.Directory]::CreateDirectory($dir) | Out-Null
}

$stream = [System.IO.MemoryStream]::new()
$writer = [System.IO.BinaryWriter]::new($stream)

# ICO header: reserved(2)=0, type(2)=1, count(2)=1
$writer.Write([UInt16]0)
$writer.Write([UInt16]1)
$writer.Write([UInt16]1)

# Single icon entry (1 image, payload is PNG data)
$writer.Write([Byte]0)      # width 0 => 256
$writer.Write([Byte]0)      # height 0 => 256
$writer.Write([Byte]0)      # color count
$writer.Write([Byte]0)      # reserved
$writer.Write([UInt16]1)    # color planes
$writer.Write([UInt16]32)   # bits per pixel
$writer.Write([UInt32]$pngBytes.Length)
$writer.Write([UInt32]22)   # offset: 6 + 16
$writer.Write($pngBytes)

$writer.Flush()
[System.IO.File]::WriteAllBytes($IcoPath, $stream.ToArray())
$writer.Dispose()
$stream.Dispose()

Write-Host "Created icon: $IcoPath"
