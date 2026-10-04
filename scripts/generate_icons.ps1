# SPDX-License-Identifier: GPL-3.0-or-later
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$svg = Join-Path $repo 'cpp\resources\icons\musubi-icon.svg'
$pngRoot = Join-Path $repo 'cpp\resources\icons\png'
$icoPath = Join-Path $repo 'cpp\resources\icons\musubi.ico'
$inkscapeCommand = Get-Command inkscape.com, inkscape.exe -ErrorAction SilentlyContinue | Select-Object -First 1
$inkscape = if ($inkscapeCommand) { $inkscapeCommand.Source } else { 'C:\Program Files\Inkscape\bin\inkscape.com' }

if (-not (Test-Path -LiteralPath $inkscape -PathType Leaf)) {
    throw 'Inkscape não foi encontrado. Instale Inkscape 1.4.4 pela página oficial https://inkscape.org/release/inkscape-1.4.4/windows/64-bit/msi/ e execute novamente.'
}
if (-not (Test-Path -LiteralPath $svg -PathType Leaf)) {
    throw "SVG de origem não encontrado: $svg"
}

$sizes = @(16, 32, 48, 64, 128, 256, 512)
foreach ($size in $sizes) {
    $directory = Join-Path $pngRoot ("{0}x{0}" -f $size)
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
    $png = Join-Path $directory 'musubi.png'
    & $inkscape $svg "--export-filename=$png" "--export-width=$size" "--export-height=$size"
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $png -PathType Leaf)) {
        throw "Inkscape falhou ao gerar o PNG de ${size}px."
    }
}

# ICO container com as seis imagens PNG exigidas pelo Windows.
$icoSizes = @(16, 32, 48, 64, 128, 256)
$pngBytes = [Collections.Generic.List[byte[]]]::new()
foreach ($size in $icoSizes) {
    $pngBytes.Add([IO.File]::ReadAllBytes((Join-Path (Join-Path $pngRoot ("{0}x{0}" -f $size)) 'musubi.png')))
}
$stream = [IO.File]::Create($icoPath)
$writer = [IO.BinaryWriter]::new($stream)
try {
    $writer.Write([UInt16]0)
    $writer.Write([UInt16]1)
    $writer.Write([UInt16]$icoSizes.Count)
    $offset = 6 + (16 * $icoSizes.Count)
    for ($i = 0; $i -lt $icoSizes.Count; $i++) {
        $size = $icoSizes[$i]
        $writer.Write([byte]$(if ($size -eq 256) { 0 } else { $size }))
        $writer.Write([byte]$(if ($size -eq 256) { 0 } else { $size }))
        $writer.Write([byte]0)
        $writer.Write([byte]0)
        $writer.Write([UInt16]1)
        $writer.Write([UInt16]32)
        $writer.Write([UInt32]$pngBytes[$i].Length)
        $writer.Write([UInt32]$offset)
        $offset += $pngBytes[$i].Length
    }
    foreach ($bytes in $pngBytes) { $writer.Write($bytes) }
}
finally {
    $writer.Dispose()
}

Write-Host "Ícones gerados em $pngRoot e $icoPath"
