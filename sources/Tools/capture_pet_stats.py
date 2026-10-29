#!/usr/bin/env python3
"""Capture real pet/attribute previews with disposable saves and normal GL rendering.
Run under Xvfb. Pillow is a report-only dependency, never a game dependency.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from PIL import Image, ImageDraw
from run_binding_previews import hashes


def pets(client):
    result=[]
    for line in (client/'scripts/table/iteminfo.txt').read_bytes().decode('latin1').splitlines():
        c=line.split('\t')
        if len(c)>90 and c[0].isdigit() and c[10]=='59':
            result.append((int(c[0]),c[1],c[90]))
    return sorted(result)


def capture(binary,client,out,all_pets=False,stats_only=False):
    binary,client,out=(Path(p).resolve() for p in (binary,client,out))
    if client==out or client in out.parents:raise ValueError('output must be outside Client')
    out.mkdir(parents=True,exist_ok=False)
    catalog=pets(client)
    cases=[('stats-'+s,['--stats-preview',s],.7) for s in ('base','necklace','berserk','harden','fairy','fairy-exhausted','passives')]
    cases += [('walk-'+s,['--stats-preview',s,'--preview-walk'],.5) for s in ('base','necklace')]
    chosen=[] if stats_only else catalog if all_pets else [r for r in catalog if r[0] in (183,184,231)]
    cases += [('pet-'+str(i),['--pet-preview',str(i)],1.2) for i,_,_ in chosen]
    cases += [('pet-183-loop',['--pet-preview','183'],6.),
              ('pet-selector',['--pet-selector','--pet-preview','183'],.7),
              ('pet-off',['--pet-preview','0'],1.2)]
    original=hashes(client/'user');original_offline=hashes(client.parent/'offline')
    env={k:v for k,v in os.environ.items() if not k.startswith('TOP_')}
    env.update(LIBGL_ALWAYS_SOFTWARE='1',SDL_AUDIODRIVER='dummy')
    report=dict(binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
                scope='preview_channels_and_RAM_only_stat_scenarios',pet_catalog=catalog,runs=[])
    with tempfile.TemporaryDirectory(prefix='top-pet-stats-') as session:
        work=Path(session);(work/'Client').mkdir()
        for src in client.iterdir():
            dst=work/'Client'/src.name
            if src.name=='user':shutil.copytree(src,dst)
            else:dst.symlink_to(src,target_is_directory=src.is_dir())
        (work/'Client/user').mkdir(exist_ok=True)
        for name in ('arquivo','offline'):
            src=client.parent/name
            if src.exists():
                if name=='offline':shutil.copytree(src,work/name)
                else:(work/name).symlink_to(src,target_is_directory=True)
        for name,options,seconds in cases:
            (work/'Client/user/offline-location-1.txt').write_text('TOPLOC1 garner 2222.5 2770.5 0.0 0\n')
            shot=out/(name+'.ppm');log=out/(name+'.log')
            command=[str(binary),'--character','1',*options,'--visual-preview-time',str(seconds),'--shot',str(shot)]
            with log.open('w') as f:
                try:code=subprocess.run(command,cwd=work,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=120).returncode
                except subprocess.TimeoutExpired:code=124
            text=log.read_text(errors='replace')
            row=dict(name=name,options=options,seconds=seconds,exit_code=code,shot_exists=shot.is_file())
            for prefix,key in (('[stats-preview-final] ','stats'),):
                lines=[l[len(prefix):] for l in text.splitlines() if l.startswith(prefix)]
                if lines:row[key]=json.loads(lines[-1])
            lines=[l for l in text.splitlines() if l.startswith(('[movement-preview]','[pet-preview]'))]
            row['observations']=lines
            if shot.is_file():
                row['ppm_sha256']=hashlib.sha256(shot.read_bytes()).hexdigest()
                Image.open(shot).save(shot.with_suffix('.png'));shot.unlink()
            report['runs'].append(row)
            report['original_user_unchanged']=hashes(client/'user')==original
            report['original_offline_unchanged']=hashes(client.parent/'offline')==original_offline
            (out/'runs.json').write_text(json.dumps(report,indent=2)+'\n')
            print(name,code,row.get('stats',lines),flush=True)
            if code or not row['shot_exists'] or not report['original_user_unchanged'] or not report['original_offline_unchanged']:
                raise RuntimeError('capture failed or original files changed; see runs.json')
    # Every full-resolution screenshot remains available. Contact thumbnails
    # use identical crops, without repainting pets or changing their exposure.
    pet_rows=[r for r in report['runs'] if re.fullmatch(r'pet-\d+',r['name'])]
    sheet=Image.new('RGB',(1250,max(1,(len(pet_rows)+4)//5)*235),'#151a22');draw=ImageDraw.Draw(sheet)
    names={i:n for i,n,_ in catalog}
    for index,row in enumerate(pet_rows):
        im=Image.open(out/(row['name']+'.png')).crop((370,100,910,550));im.thumbnail((245,205))
        x=index%5*250;y=index//5*235;sheet.paste(im,(x,y+28));draw.text((x+5,y+4),row['name']+' '+names[int(row['name'][4:])][:20],fill='white')
    if pet_rows:sheet.save(out/'pets-contact.jpg',quality=93)
    blocks=[]
    for row in report['runs']:
        caption=row['name']+' @ '+str(row['seconds'])+'s'
        if 'stats' in row:caption+=' — '+json.dumps(row['stats'],ensure_ascii=False)
        blocks.append('<article><h2>'+caption+'</h2><a href="'+row['name']+'.png"><img loading="lazy" src="'+row['name']+'.png"></a></article>')
    (out/'index.html').write_text('''<!doctype html><html lang="pt-BR"><meta charset="utf-8"><title>Pets e atributos — SK2.20</title><style>body{max-width:1280px;margin:24px auto;background:#151a22;color:#edf4fa;font:16px system-ui}h2{font-size:16px}article{padding:16px;margin:22px 0;background:#243041}img{max-width:100%}a{color:#8dd5ff}</style><h1>Pets e atributos — capturas reais</h1><p>Alt+Y abre a escolha de qualquer fada, sem inventário ou bônus. “Usar a fada equipada” restaura a aparência normal. As outras imagens usam cenários descartáveis de atributos; seus valores são os mesmos consultados pelo combate e pela ficha. Estas capturas não certificam fidelidade ao DX9.</p>'''+''.join(blocks)+'</html>')
    return report


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary',type=Path,default=Path('build/top_gl'))
    parser.add_argument('--client',type=Path,default=Path('Client'))
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--all-pets',action='store_true')
    parser.add_argument('--stats-only',action='store_true')
    a=parser.parse_args();capture(a.binary,a.client,a.out,a.all_pets,a.stats_only)
