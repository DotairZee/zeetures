# Zeetures's program icons

For now the icons are the egg from openc2e's 2008 logo: the same 128 x 128
picture as the window icon (`src/sdlbackend/openc2e_icon.h`), scaled up.

- `zeetures.png` (256 x 256): Linux (the AppImage and its menu entry)
- `zeetures.ico` (16 to 256): Windows (Zeetures.exe, the installer, shortcuts)
- `zeetures.icns` (16 to 512): macOS (Zeetures.app)

They were made from the window icon with Pillow:

```python
import re
from PIL import Image
src = open("src/sdlbackend/openc2e_icon.h").read()
body = src[src.index("{", src.index("openc2e_icon_rgba")) + 1:src.rindex("}")]
im = Image.frombytes("RGBA", (128, 128), bytes(int(v) for v in re.findall(r"\d+", body)))
big = im.resize((256, 256), Image.LANCZOS)
big.save("packaging/icons/zeetures.png")
big.save("packaging/icons/zeetures.ico", sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
im.resize((512, 512), Image.LANCZOS).save("packaging/icons/zeetures.icns")
```
