OBJ_DIR := obj

.PHONY: all clean run run-sound

all:
	powershell -ExecutionPolicy Bypass -File .\\build.ps1

run: all
	qemu-system-i386 -drive format=raw,file=$(OBJ_DIR)/inter-minux.img

run-sound: all
	qemu-system-i386 -audiodev dsound,id=snd0 -machine pcspk-audiodev=snd0 -drive format=raw,file=$(OBJ_DIR)/inter-minux.img

clean:
	if exist $(OBJ_DIR) rmdir /s /q $(OBJ_DIR)
