# Zeetures's program icons

The icon is an egg with a heartbeat: a pale pink core glowing inside a mauve
shell with a plum outline, and a pulse line across the core whose ends run
under the shell's inner edge. It is drawn from scratch, with no game art, by
`art/app_icon.py` (design F7 there; the other designs tried are in the same
file).

- `zeetures.png` (256 x 256): Linux (the AppImage and its menu entry)
- `zeetures.ico` (16 to 256): Windows (Zeetures.exe, the installer, shortcuts)
- `zeetures.icns` (16 to 512): macOS (Zeetures.app)

The window icon, `src/sdlbackend/openc2e_icon.h`, is the same picture at
128 x 128. To redraw all four after changing the design (it needs Pillow and
NumPy):

```sh
python3 art/app_icon.py --install
```
