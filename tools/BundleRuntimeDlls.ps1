param(
    [Parameter(Mandatory = $true)]
    [string]$OutDir,
    [Parameter(Mandatory = $true)]
    [string]$BinPaths,
    [string]$RootBinary = "LightQuest.exe"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $OutDir)) {
    throw "Release directory not found: $OutDir"
}

$rootBinaryPath = Join-Path $OutDir $RootBinary
if (-not (Test-Path -LiteralPath $rootBinaryPath)) {
    throw "Missing binary: $rootBinaryPath"
}

$binPathList = New-Object "System.Collections.Generic.List[string]"
foreach ($rawPath in ($BinPaths -split ";")) {
    if ($rawPath -and (Test-Path -LiteralPath $rawPath) -and (-not $binPathList.Contains($rawPath))) {
        [void]$binPathList.Add($rawPath)
    }
}

if ($binPathList.Count -eq 0) {
    throw "No valid runtime bin folders were provided."
}

$objdumpPath = $null
foreach ($path in $binPathList) {
    $candidate = Join-Path $path "objdump.exe"
    if (Test-Path -LiteralPath $candidate) {
        $objdumpPath = $candidate
        break
    }
}

if (-not $objdumpPath) {
    $objdumpCmd = Get-Command objdump.exe -ErrorAction SilentlyContinue
    if ($objdumpCmd) {
        $objdumpPath = $objdumpCmd.Source
    }
}

if (-not $objdumpPath) {
    throw "objdump.exe not found. Install mingw-w64-binutils in your active MSYS2 toolchain."
}

$systemDlls = @(
    "advapi32.dll", "bcrypt.dll", "combase.dll", "comdlg32.dll", "gdi32.dll", "imm32.dll",
    "kernel32.dll", "msvcrt.dll", "ntdll.dll", "ole32.dll", "oleaut32.dll", "opengl32.dll",
    "rpcrt4.dll", "setupapi.dll", "shell32.dll", "shlwapi.dll", "user32.dll", "uxtheme.dll",
    "version.dll", "winmm.dll", "ws2_32.dll", "dwrite.dll", "usp10.dll"
)

$systemSet = New-Object "System.Collections.Generic.HashSet[string]" ([System.StringComparer]::OrdinalIgnoreCase)
foreach ($dll in $systemDlls) {
    [void]$systemSet.Add($dll)
}

$queue = New-Object "System.Collections.Generic.Queue[string]"
$scannedBinaries = New-Object "System.Collections.Generic.HashSet[string]" ([System.StringComparer]::OrdinalIgnoreCase)
$copiedDlls = New-Object "System.Collections.Generic.HashSet[string]" ([System.StringComparer]::OrdinalIgnoreCase)
$missingDlls = New-Object "System.Collections.Generic.HashSet[string]" ([System.StringComparer]::OrdinalIgnoreCase)
$queue.Enqueue($rootBinaryPath)

while ($queue.Count -gt 0) {
    $binaryPath = $queue.Dequeue()
    if (-not $scannedBinaries.Add($binaryPath)) {
        continue
    }

    $dumpLines = & $objdumpPath -p $binaryPath
    foreach ($line in $dumpLines) {
        if ($line -notmatch "DLL Name:\s*(.+)$") {
            continue
        }

        $dllName = $Matches[1].Trim()
        if (-not $dllName) {
            continue
        }
        if ($dllName -like "api-ms-win-*.dll" -or $dllName -like "ext-ms-win-*.dll") {
            continue
        }
        if ($systemSet.Contains($dllName)) {
            continue
        }

        $foundPath = $null
        foreach ($bin in $binPathList) {
            $candidateDll = Join-Path $bin $dllName
            if (Test-Path -LiteralPath $candidateDll) {
                $foundPath = $candidateDll
                break
            }
        }

        if (-not $foundPath) {
            [void]$missingDlls.Add($dllName)
            continue
        }

        if ($copiedDlls.Add($dllName)) {
            $destPath = Join-Path $OutDir $dllName
            try {
                Copy-Item -LiteralPath $foundPath -Destination $destPath -Force -ErrorAction Stop
            }
            catch {
                Start-Sleep -Milliseconds 200
                Copy-Item -LiteralPath $foundPath -Destination $destPath -Force -ErrorAction Stop
            }
            Write-Host ("Bundled: " + $dllName)
        }

        $queue.Enqueue($foundPath)
    }
}

if ($missingDlls.Count -gt 0) {
    foreach ($missing in $missingDlls) {
        Write-Host ("Missing runtime DLL (not found in configured bins): " + $missing)
    }
    throw "Missing runtime DLLs. Ensure all required MSYS2 packages are installed."
}

Write-Host ("Runtime bundling complete. DLL count: " + $copiedDlls.Count)
