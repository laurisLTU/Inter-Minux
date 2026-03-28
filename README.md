# Inter-Minux (Ai written OS using C)

Inter-Minux is a hobby OS prototype written with bootloader ASM, NASM, and C.

> **Alpha status:** This project is currently **alpha** and has known bugs, missing features, and rough edges.

## Features (Current)

- Custom bootloader + protected-mode kernel
- Retro desktop UI with draggable windows
- Desktop apps:
  - Calculator
  - Status
  - Minux Studio (M++)
  - File Explorer
  - About
  - Virus Panel (harmless visual demo)
- In-kernel file persistence to disk sectors (prototype)
- Basic `.mpp` / `.mex` workflow

## Requirements

- `nasm`
- `i686-elf-gcc`
- `i686-elf-ld`
- `i686-elf-objcopy`
- `qemu-system-i386`
- PowerShell (for build script)

## Build

```powershell
powershell -ExecutionPolicy Bypass -File .\build.ps1
```

## Run

```powershell
qemu-system-i386 -drive format=raw,file=obj\inter-minux.img
```

## Release Image (.img)

If you download the release `.img`, you can run it directly without building:

```powershell
qemu-system-i386 -drive format=raw,file=inter-minux.img
```

Optional sound-enabled run:

```powershell
make run-sound
```

## Development Notes

- Source code is in `src/`
- Build outputs are in `obj/`
- The boot image is generated as `obj/inter-minux.img`

## If You Want To Edit This OS

If you want to modify Inter-Minux, use the source code version (not only the release `.img`):

1. Install all requirements listed above.
2. Edit files in `src/` (for example `src/kernel.c`, `src/boot.asm`, `src/window.c`).
3. Rebuild:

```powershell
powershell -ExecutionPolicy Bypass -File .\build.ps1
```

4. Run the rebuilt image:

```powershell
qemu-system-i386 -drive format=raw,file=obj\inter-minux.img
```

## Alpha Warning / Known Issues

This is an early alpha. You should expect:

- UI glitches and rendering artifacts
- Input edge cases
- Incomplete file explorer behavior
- Incomplete language/compiler behavior in Studio
- Unstable interactions between windows/features

## License

No license set yet.
