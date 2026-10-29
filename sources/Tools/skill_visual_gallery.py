#!/usr/bin/env python3
"""Build a local before/after gallery from real previews and user reference PNGs.
Pillow is used only for the comparison report, never by the game.
"""
import argparse
import hashlib
import html
import io
import json
import re
from pathlib import Path
import zipfile

from PIL import Image, ImageDraw
from capture_skill_comparison import CASES


def build(before, after, out, berserk=None, slash=None):
    before, after, out = map(Path, (before, after, out))
    out.mkdir(parents=True, exist_ok=True)
    frames = out / 'frames'
    frames.mkdir(exist_ok=True)
    pairs, manifest = [], {'references': [], 'frames': []}
    labels = {84: 'Berserk', 81: 'Illusion Slash', 93: 'Frozen Arrow',
              90: 'Dual Shot', 242: 'Taunt'}
    # Rendered screenshots retain their pixels, size and colour. No effect
    # repainting, exposure adjustment, recentering or rescaling of either side.
    sheet = Image.new('RGB', (1280, len(CASES)*395), '#151a22')
    draw = ImageDraw.Draw(sheet)
    for row, (skill, channel, seconds) in enumerate(CASES):
        stem = f'{skill}-{channel}-{seconds}'
        title = f'{labels[skill]} ({skill}) / {channel} / {seconds}s'
        for col, (stage, folder) in enumerate((('before', before), ('after', after))):
            source = folder / (stem + '.ppm')
            if not source.is_file():
                raise ValueError('missing screenshot: ' + str(source))
            image = Image.open(source).convert('RGB')
            dest = frames / f'{stage}-{stem}.png'
            image.save(dest)
            manifest['frames'].append(dict(source=str(source),
                source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
                png=dest.relative_to(out).as_posix(), pixels=list(image.size)))
            thumb = image.copy(); thumb.thumbnail((630, 355))
            sheet.paste(thumb, (col*640, row*395+30))
            draw.text((col*640+8, row*395+8), stage+' '+title, fill='white')
        pairs.append(f'''<article><h2>{html.escape(title)}</h2>
<div class="compare"><img src="frames/after-{stem}.png" alt="Depois">
<div class="before"><img src="frames/before-{stem}.png" alt="Antes"></div></div>
<label>Antes ← <input type="range" min="0" max="100" value="50"
oninput="this.closest('article').querySelector('.before').style.clipPath='inset(0 '+(100-this.value)+'% 0 0)'"> → Depois</label>
<p><a href="frames/before-{stem}.png">Antes em tamanho real</a> ·
<a href="frames/after-{stem}.png">Depois em tamanho real</a></p></article>''')
    sheet.save(out / 'comparison-contact.jpg', quality=91)
    reference_html = []
    for label, source, root_only in [('Berserk', berserk, False), ('Illusion Slash', slash, True)]:
        if not source:
            continue
        source = Path(source)
        entries = []
        with zipfile.ZipFile(source) as z:
            for index, member in enumerate(sorted(z.namelist(), key=lambda value: [int(v) if v.isdigit() else v for v in re.split(r'(\d+)', value)])):
                if not member.lower().endswith('.png') or (root_only and '/' in member):
                    continue
                data = z.read(member)
                image = Image.open(io.BytesIO(data)).convert('RGB')
                dest = frames / f'reference-{label.replace(" ", "-")}-{index}.png'
                image.save(dest)
                entries.append(f'<a href="{dest.relative_to(out).as_posix()}"><img loading="lazy" src="{dest.relative_to(out).as_posix()}" alt="{html.escape(member)}"></a>')
                manifest['references'].append(dict(archive=str(source), member=member,
                    archive_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
                    member_sha256=hashlib.sha256(data).hexdigest(),
                    png=dest.relative_to(out).as_posix()))
        reference_html.append(f'<h2>Referência fornecida: {label}</h2><div class="refs">'+''.join(entries)+'</div>')
    (out / 'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
    (out / 'index.html').write_text('''<!doctype html><html lang="pt-BR"><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>SK2.19 — comparação de skills</title>
<style>body{margin:24px auto;max-width:1250px;padding:0 18px;background:#151a22;color:#e6ecf4;font:16px system-ui}h1{font-size:28px}h2{font-size:20px}a{color:#83cfff}article{margin:30px 0;padding:18px;background:#202937;border-radius:10px}.compare{position:relative}.compare img{width:100%;display:block}.before{position:absolute;inset:0;clip-path:inset(0 50% 0 0)}input{width:55%;margin:18px}.refs{display:grid;grid-template-columns:repeat(3,1fr);gap:10px}.refs img{width:100%}p{line-height:1.55}</style>
<h1>SK2.19 — comparação visual das skills</h1>
<p>Arraste para comparar a revisão anterior com a atual, no mesmo local, câmera e instante da prévia.
Capturas reais do OpenGL/Mesa, em passos de 1/60 s. Os canais são disparados isoladamente: estas imagens não comprovam a sequência completa do combate.</p>
<p>As referências do usuário aparecem abaixo. Câmera, resolução e instantes diferem; não há FPS confirmado nessas sequências.
A comparação com o original é qualitativa e parcial. Nenhuma skill foi certificada como visualmente idêntica.</p>
<p>Berserk: aura e feixes maiores; Illusion Slash: meia-lua recuperada; Frozen Arrow: modelo de gelo maior; Dual Shot: três componentes preservados; Taunt: estrelas preservadas.
O impacto provisório do Illusion Slash mantém 2,5× e ainda requer revisão visual de tamanho.</p>
'''+''.join(pairs)+''.join(reference_html)+'</html>')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for field in ('before', 'after', 'out'):
        parser.add_argument('--'+field, type=Path, required=True)
    parser.add_argument('--berserk-zip', type=Path)
    parser.add_argument('--slash-zip', type=Path)
    args = parser.parse_args()
    build(args.before, args.after, args.out, args.berserk_zip, args.slash_zip)
