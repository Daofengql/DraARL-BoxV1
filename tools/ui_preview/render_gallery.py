"""Convert native LVGL renders to PNG, enlarged views and a local gallery."""
import html
import math
from pathlib import Path
import sys

from PIL import Image, ImageDraw, ImageFont

folder = Path(sys.argv[1] if len(sys.argv) > 1 else "build/ui-preview/details")
rows = [line.split("\t", 2) for line in (folder / "scenes.tsv").read_text(encoding="utf-8").splitlines()]
font_path = Path("C:/Windows/Fonts/msyh.ttc")
font = ImageFont.truetype(str(font_path), 15) if font_path.exists() else ImageFont.load_default()

for name, _, _ in rows:
    with Image.open(folder / f"{name}.ppm") as image:
        image.save(folder / f"{name}.png")
        image.resize((960, 720), Image.Resampling.NEAREST).save(folder / f"{name}@3x.png")

def sheet(entries, path, columns=3):
    canvas = Image.new("RGB", (columns * 336 + 16, math.ceil(len(entries) / columns) * 284 + 16), "#0e151f")
    draw = ImageDraw.Draw(canvas)
    for index, (name, title, _) in enumerate(entries):
        x, y = 16 + index % columns * 336, 16 + index // columns * 284
        draw.text((x, y), f"{name[:2]}  {title}", fill="#e6edf5", font=font)
        with Image.open(folder / f"{name}.png") as image:
            canvas.paste(image, (x, y + 28))
    canvas.save(path)

for start in range(0, len(rows), 6):
    sheet(rows[start:start + 6], folder / f"sheet-{start // 6 + 1:02}.png")
sheet(rows, folder / "contact.png")

cards = []
links = []
for name, title, detail in rows:
    name, title, detail = map(html.escape, (name, title, detail))
    links.append(f'<a href="#{name}">{name[:2]} · {title}</a>')
    cards.append(f'''<article id="{name}"><h2>{name[:2]} · {title}</h2>
<a href="{name}@3x.png" target="_blank"><img src="{name}.png" width="320" height="240" alt="{title}" loading="lazy"></a>
<p>{detail}</p><a class="original" href="{name}@3x.png" target="_blank">打开 3 倍原图 ↗</a></article>''')

page = '''<!doctype html><html lang="zh-CN"><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>BoxV1 · LVGL 详细页面</title>
<style>
*{box-sizing:border-box}html{scroll-behavior:smooth;scroll-padding-top:20px}
body{margin:0;background:#0e151f;color:#e6edf5;font:15px/1.7 system-ui,sans-serif}
a{color:#4ad8e8;text-decoration:none}a:hover{text-decoration:underline}
header{padding:36px max(24px,4vw) 28px;border-bottom:1px solid #2a3b4d}
h1{font-size:30px;line-height:1.3;margin:0 0 12px}header p{max-width:920px;color:#a8bbce;margin:8px 0}
.rules{display:flex;gap:10px;flex-wrap:wrap;margin-top:20px}.rules span{background:#182331;border:1px solid #2a3b4d;border-radius:6px;padding:6px 12px}
.selected{color:#4ad8e8}.editing{color:#ff8a38}.layout{display:grid;grid-template-columns:220px 1fr;gap:24px;padding:24px max(24px,4vw)}
nav{position:sticky;top:20px;align-self:start;max-height:calc(100vh - 40px);overflow:auto;font-size:13px}
nav a{display:block;padding:5px 8px;border-left:1px solid #2a3b4d;color:#a8bbce}nav a:hover{color:#4ad8e8}
main{display:grid;grid-template-columns:repeat(auto-fit,minmax(min(100%,420px),1fr));gap:24px;align-items:start}
article{background:#182331;border:1px solid #2a3b4d;padding:20px;border-radius:10px;min-width:0}
h2{font-size:18px;margin:0 0 16px}article img{display:block;width:100%;height:auto;image-rendering:pixelated;background:#000;border-radius:3px}
article p{color:#a8bbce;font-size:14px;min-height:48px}.original{font-size:13px}
@media(max-width:850px){.layout{display:block}nav{display:none}main{grid-template-columns:1fr}header{padding-top:24px}}
</style><header><h1>BoxV1 · LVGL 详细页面</h1>
<p>29 个页面与操作状态，使用固件实际 LVGL、编辑控制器及编码器代码离屏渲染。点击画面可查看 960 × 720 原图。</p>
<div class="rules"><span>旋转：选择 / 调值</span><span>按下：进入 / 确认</span><span>KEY0：返回 / 取消</span><span>长按 KEY0：电源菜单</span><span class="selected">青色：选中</span><span class="editing">橙色：编辑</span></div>
<p>频率选位包含 Save，长按旋钮也可保存。这里展示已渲染的状态；GPIO、真实屏幕和 BLE 仍需设备联调。</p>
</header><div class="layout"><nav>__LINKS__</nav><main>__CARDS__</main></div></html>'''
(folder / "index.html").write_text(page.replace("__LINKS__", "\n".join(links)).replace("__CARDS__", "\n".join(cards)), encoding="utf-8")
print(f"Exported {len(rows)} PNGs, 3x views, {math.ceil(len(rows) / 6)} sheets and {folder / 'index.html'}")
