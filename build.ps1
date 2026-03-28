$ErrorActionPreference = 'Stop'

$Obj = 'obj'
$Src = 'src'

$nasm = Get-Command nasm -ErrorAction SilentlyContinue
$gcc = Get-Command i686-elf-gcc -ErrorAction SilentlyContinue
$ld  = Get-Command i686-elf-ld -ErrorAction SilentlyContinue
$objcopy = Get-Command i686-elf-objcopy -ErrorAction SilentlyContinue

if (-not $nasm) { throw 'nasm not found in PATH.' }
if (-not $gcc)  { throw 'i686-elf-gcc not found in PATH.' }
if (-not $ld)   { throw 'i686-elf-ld not found in PATH.' }
if (-not $objcopy) { throw 'i686-elf-objcopy not found in PATH.' }

New-Item -ItemType Directory -Force $Obj | Out-Null

$bootBin = Join-Path $Obj 'boot.bin'
$entryObj = Join-Path $Obj 'kernel_entry.o'
$kernelObj = Join-Path $Obj 'kernel.o'
$windowObj = Join-Path $Obj 'window.o'
$kernelElf = Join-Path $Obj 'kernel.elf'
$kernelBin = Join-Path $Obj 'kernel.bin'
$img = Join-Path $Obj 'inter-minux.img'
$kernelSectorsInc = Join-Path $Src 'kernel_sectors.inc'

& nasm -f elf32 (Join-Path $Src 'kernel_entry.asm') -o $entryObj
& i686-elf-gcc -m32 -ffreestanding -fno-pie -fno-stack-protector -nostdlib -nostdinc -Wall -Wextra -O2 -c (Join-Path $Src 'window.c') -o $windowObj
& i686-elf-gcc -m32 -ffreestanding -fno-pie -fno-stack-protector -nostdlib -nostdinc -Wall -Wextra -O2 -c (Join-Path $Src 'kernel.c') -o $kernelObj
& i686-elf-ld -m elf_i386 -T (Join-Path $Src 'linker.ld') -nostdlib -o $kernelElf $entryObj $windowObj $kernelObj
& i686-elf-objcopy -O binary $kernelElf $kernelBin

$kernelLen = (Get-Item $kernelBin).Length
$sectors = [int][Math]::Ceiling($kernelLen / 512.0)
if ($sectors -lt 1) { $sectors = 1 }
$sectorLine = 'KERNEL_SECTORS equ ' + $sectors
[System.IO.File]::WriteAllText((Join-Path (Get-Location) $kernelSectorsInc), $sectorLine, [System.Text.Encoding]::ASCII)

& nasm -f bin (Join-Path $Src 'boot.asm') -o $bootBin

$bootBytes = [System.IO.File]::ReadAllBytes((Resolve-Path $bootBin))
$kernelBytes = [System.IO.File]::ReadAllBytes((Resolve-Path $kernelBin))
$all = New-Object byte[] ($bootBytes.Length + $kernelBytes.Length)
[System.Buffer]::BlockCopy($bootBytes, 0, $all, 0, $bootBytes.Length)
[System.Buffer]::BlockCopy($kernelBytes, 0, $all, $bootBytes.Length, $kernelBytes.Length)
[System.IO.File]::WriteAllBytes((Join-Path (Get-Location) $img), $all)

Write-Host "Built $img (kernel sectors: $sectors)"
