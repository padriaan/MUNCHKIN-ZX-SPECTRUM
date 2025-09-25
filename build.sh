zcc +zx -vn -startup=31 -DWFRAMES=3 -clib=sdcc_iy -SO3 --max-allocs-per-node10000 --fsigned-char @zproject.lst -o munchkin -pragma-include:zpragma.inc

z88dk.z88dk-appmake +zx -b munchkin_CODE.bin -o game.tap --blockname game --org 25124 --noloader

cat loader.tap screen.tap  game.tap > munchkin_z80.tap

# optional
# zxtap2wav-1.0.3-linux-amd64 -a -i munchkin_z80.tap munchkin_z80.wav
