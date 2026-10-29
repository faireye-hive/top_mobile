#!/usr/bin/env python3
"""Rebuild coverage from CURRENT tables/assets and the compiled readers.
Standard library only. No resource mutation and no claim of visual validation.
The legacy blend audit is used for raw inventory only; its old implementation
labels predate SK2.6 and MUST NOT be used to classify the current GL renderer.
"""
import argparse
import collections
import csv
import hashlib
import json
from pathlib import Path
import re
import subprocess
import struct

PRIMITIVES = {'Triangle', 'TrianglePlane', 'Rect', 'RectPlane', 'RectZ', 'Cone', 'Cylinder'}
TYPES = dict(enumerate(['SNOW', 'FIRE', 'BLAST', 'RIPPLE', 'MODEL', 'STRIP', 'WIND',
                       'ARRAW', 'ROUND', 'BLAST2', 'BLAST3', 'SHRINK', 'SHADE',
                       'RANGE', 'RANGE2', 'DUMMY', 'LINE_SINGLE', 'LINE_ROUND'], 1))
MODES = ['Part_drop', 'Part_fly', 'Part_trace', 'Part_fshade', 'Part_arc', 'Part_dirlight', 'Part_dist']


def rows(path):
    # latin1 is a lossless view of historical bytes, also used by older audits.
    with Path(path).open(encoding='latin1', newline='') as f:
        return list(csv.DictReader(f))


def table(path):
    result = {}
    for line in Path(path).read_bytes().decode('latin1').splitlines():
        c = line.split('\t')
        if c[0].isdigit() and int(c[0]) > 0:
            result[int(c[0])] = c
    return result


def num(s):
    try:
        return int(s)
    except ValueError:
        return 0


def names(s):
    return [n.strip() for n in s.split(',') if n.strip() not in ('', '0')]


def write_csv(path, fields, data):
    with Path(path).open('w', encoding='utf-8', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        writer.writerows(data)


def audit(client, build, out, runtime_evidence=None):
    client, build, out = Path(client), Path(build).resolve(), Path(out)
    out.mkdir(parents=True, exist_ok=True)
    evidence={}
    if runtime_evidence:
        evidence_path=Path(runtime_evidence)
        evidence=json.loads(evidence_path.read_text())
        for record in evidence.values():
            for file in record.get('files',[]):
                if not (evidence_path.parent/file).is_file():
                    raise ValueError('missing runtime evidence: '+file)
    for exe, folder in [('top_par_audit', 'par'), ('top_blend_audit', 'blend')]:
        run = subprocess.run([str(build / exe), '--client', str(client), '--out', str(out / folder)],
                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=True)
        (out / (folder + '_audit.log')).write_bytes(run.stdout)
    subprocess.run([str(build / 'top_skill_fx_probe'), str(client), str(out / 'cpu_probe.csv')], check=True)
    systems = collections.defaultdict(list)
    for row in rows(out / 'par/par_systems.csv'):
        systems[row['file']].append(row)
    uses = collections.defaultdict(list)
    for row in rows(out / 'blend/blend_usage.csv'):
        uses[row['file']].append(row)
    par_files = {r['file']: r for r in rows(out / 'par/par_files.csv')}
    failed_eff = {r['file'] for r in rows(out / 'blend/blend_failures.csv') if r['kind'] == 'eff'}
    # CMPPartCtrl appends direct CMPStrip/CChaModel tracks after the systems.
    # They are NOT the particle type STRIP=6, and the runtime ignores them.
    controller_tails = {}
    for rel, record in par_files.items():
        data=(client/rel).read_bytes()
        at=num(record['consumed_bytes'])
        version=num(record['version'])
        tracks=[]
        if version>=7 and at+4<=len(data):
            count=struct.unpack_from('<i',data,at)[0];at+=4
            if count<0 or count>100 or at+count*76>len(data):
                raise ValueError('invalid CMPStrip tail: '+rel)
            for i in range(count):
                values=struct.unpack_from('<3i6f32s2i',data,at);at+=76
                tracks.append(dict(file=rel,index=i,max_length=values[0],dummy_a=values[1],dummy_b=values[2],
                                   life=values[7],step=values[8],
                                   texture=values[9].split(b'\0',1)[0].decode('latin1'),
                                   src_blend=values[10],dst_blend=values[11],implementation='not_rendered'))
            models=0
            if version>=8 and at+4<=len(data):
                models=struct.unpack_from('<i',data,at)[0];at+=4
                if models<0 or models>100 or at+models*40>len(data):
                    raise ValueError('invalid CChaModel tail: '+rel)
                at+=models*40
            controller_tails[rel]=(tracks,models,len(data)-at)
    direct_strips=[r for tracks,_,_ in controller_tails.values() for r in tracks]
    if direct_strips:
        write_csv(out/'DIRECT_CONTROLLER_STRIPS.csv',list(direct_strips[0]),direct_strips)
    probe = {int(r['skill_id']): r for r in rows(out / 'cpu_probe.csv')}
    skills = table(client / 'scripts/table/skillinfo.txt')
    magic = table(client / 'scripts/table/magicsingleinfo.txt')
    groups = table(client / 'scripts/table/magicgroupinfo.txt')
    scenes = table(client / 'scripts/table/sceneffectinfo.txt')
    assets = {p.relative_to(client).as_posix(): p for p in (client / 'effect').rglob('*') if p.is_file()}
    texture_manifest = {}
    manifest_path = client / 'texture/texture_manifest.csv'
    if manifest_path.exists():
        for r in rows(manifest_path):
            values = list(r.values())
            if len(values) >= 2:
                texture_manifest[values[0].lower()] = values[1]

    def texture(name):
        stem = name.rsplit('.', 1)[0] if '.' in name else name
        for candidate in [name, stem + '.tga', stem + '.dds', stem + '.png']:
            key = 'effect/' + candidate
            mapped = texture_manifest.get(key.lower())
            if mapped and (client / 'texture' / mapped).is_file():
                return 'texture/' + mapped
            for path in ['texture/' + key, 'texture/' + key + '.png']:
                if (client / path).is_file():
                    return path
        return None

    inspection = {}
    for rel, path in sorted(assets.items()):
        if path.suffix.lower() != '.eff' or rel in failed_eff:
            continue
        # The existing compiled reader exposes verified filename fields,
        # including texture animation frames omitted by blend_usage.csv.
        run = subprocess.run([str(build / 'top_eff_inspect'), str(path)], stdout=subprocess.PIPE, check=True)
        text = run.stdout.decode('latin1')
        refs = text.split('Recursos em campos identificados: ', 1)
        inspection[rel] = dict(refs=[r.strip() for r in refs[1].splitlines()[1:]] if len(refs) == 2 else [],
                               path='caminho (nao)' not in text,
                               rotating=path.read_bytes()[74]!=0,
                               # useSound is diagnostic only; filenames retained below
                               details=text)
    par_deps = collections.defaultdict(list)
    for row in rows(out / 'par/par_dependencies.csv'):
        par_deps[row['file']].append(row)

    def closure(roots):
        required, missing, unread, unsupported, types = set(), set(), set(), set(), set()
        todo = list(roots)
        while todo:
            rel = todo.pop()
            if rel in required:
                continue
            required.add(rel)
            if rel not in assets:
                missing.add(rel)
                continue
            if rel.endswith('.par'):
                record = par_files.get(rel)
                if not record or record['status'] not in ('decoded','empty_controller'):
                    unread.add(rel)
                    continue
                strips, character_models, remaining=controller_tails.get(rel,([],0,num(record['trailing_bytes'])))
                if strips:
                    unsupported.add('direct_CMPStrip_dummy_trail_not_rendered')
                    for strip in strips:
                        found=texture(strip['texture'])
                        if found:required.add(found)
                        else:missing.add('texture/effect/'+strip['texture'])
                if character_models:
                    unsupported.add('direct_CChaModel_not_rendered')
                if remaining:
                    unsupported.add('uninterpreted_par_controller_tail')
                for s in systems[rel]:
                    t = num(s['type'])
                    types.add(t)
                    model = s['model_name']
                    if model.lower().endswith('.eff'):
                        todo.append('effect/' + model)
                        if t == 13:
                            unsupported.add('SHADE_uses_ground_controller_instead_of_model')
                    elif model not in PRIMITIVES and model:
                        unsupported.add('particle_model_' + model)
                    if t not in (5, 6):
                        unsupported.add('approximate_particle_' + TYPES.get(t, str(t)))
                    if num(s['count_raw']) > 24 and t not in (5, 6):
                        unsupported.add('particle_pool_capped_at_24')
                    if s['model_range'] == '1':
                        unsupported.add('model_range_emission')
                    if t in (5, 6):
                        unsupported.add('actor_bone_world_matrix_binding')
                for d in par_deps[rel]:
                    if d['kind'] == 'texture':
                        # Par loader lowercases and appends .tga when extension absent.
                        name = d['declared_name'].lower()
                        name += '.tga' if '.' not in name else ''
                        found = texture(name)
                        if found:
                            required.add(found)
                        else:
                            missing.add('texture/effect/' + name)
                    elif d['kind'] == 'model_eff':
                        todo.append('effect/' + d['declared_name'])
            elif rel.endswith('.eff'):
                if rel in failed_eff:
                    unread.add(rel)
                    continue
                info = inspection.get(rel, {})
                if info.get('path') or info.get('rotating'):
                    unsupported.add('eff_document_path_or_rotation_requires_review')
                for u in uses[rel]:
                    model = u['model']
                    if model not in PRIMITIVES and u['billboard'] != '1':
                        lgo = model if model.lower().endswith('.lgo') else model + '.lgo'
                        key = 'model/effect/' + lgo
                        required.add(key)
                        if not (client / key).is_file():
                            missing.add(key)
                        else:
                            unsupported.add('lgo_materials_and_animation_require_visual_review')
                for ref in info.get('refs', []):
                    if ref in PRIMITIVES or ref in {u['model'] for u in uses[rel]}:
                        continue
                    if ref.lower().endswith(('.eff', '.par')):
                        todo.append('effect/' + ref)
                    elif ref.lower().endswith(('.wav', '.ogg')):
                        unsupported.add('eff_sound_dispatch')
                    else:
                        found = texture(ref)
                        if found:
                            required.add(found)
                        else:
                            # A document path is not a texture. Keep unresolved
                            # references distinct from confirmed missing images.
                            if ref.lower().endswith(('.tga', '.dds', '.bmp', '.png')):
                                missing.add('texture/effect/' + ref)
                            else:
                                unsupported.add('unresolved_eff_reference_' + ref)
        return required, missing, unread, unsupported, types

    result = []
    skill_mode_counts = collections.Counter()
    for sid, c in sorted(skills.items()):
        if len(c) <= 66:
            raise ValueError(f'truncated skill row {sid}')
        ids = []
        for n in names(c[51])[:2]:
            if num(n) <= 0:
                break
            ids.append(num(n))
        ids += [num(c[i].split(',')[0]) for i in (54, 55, 59, 63, 65, 66)]
        ids = [n for n in ids if n > 0]
        roots, missing_table, modes = set(), set(), set()
        pending = list(ids)
        while pending:
            vid = pending.pop()
            if 2000 <= vid < 3000:
                g = groups.get(vid - 2000)
                if not g:
                    missing_table.add('magicgroup:' + str(vid - 2000))
                else:
                    pending += [1000 + num(m) for m in names(g[3])]
                continue
            if vid in scenes:
                roots.add('effect/' + scenes[vid][1])
            mid = vid - 1000 if 1000 <= vid < 1100 else vid
            if mid in magic:
                m = magic[mid]
                modes.add(num(m[8]))
                roots.update('effect/' + n for n in names(m[3]))
                roots.update('effect/' + (n if n.endswith('.par') else n + '.par') for n in names(m[6]))
                for n in names(m[10]):
                    filename = n if n.endswith('.par') else n + '.par'
                    if 'effect/' + filename not in assets and n.isdigit() and len(n) < 8:
                        padded = n.zfill(8) + '.par'
                        if 'effect/' + padded in assets:
                            filename = padded
                    roots.add('effect/' + filename)
            elif vid not in scenes:
                missing_table.add('visual:' + str(vid))
        required, missing, unread, unsupported, types = closure(roots)
        missing.update(missing_table)
        if ids:
            unsupported.update(['effect_dummy_world_matrix_binding', 'per_channel_animation_events',
                                'render_order_ztest_cull_sampler_review', 'character_lab_action_events'])
        if modes.intersection({0, 3, 4}):
            unsupported.add('terrain_collision')
        if 2 in modes:
            unsupported.add('moving_target_trace_and_1_5_step_cap')
        if 6 in modes:
            unsupported.add('authored_range_override')
        if 3 in modes:
            unsupported.add('fshade_frame_order_and_copy_cap')
        for mode in modes:
            skill_mode_counts[mode] += 1
        p = probe.get(sid, {})
        result.append(dict(skill_id=sid, name=c[1], play_actions=c[47], resources_required='|'.join(sorted(required)),
                           historical_modes='|'.join(f'{m}:{MODES[m]}' if 0 <= m <= 6 else str(m) for m in sorted(modes)),
                           particle_types='|'.join(f'{t}:{TYPES.get(t,"UNKNOWN")}' for t in sorted(types)),
                           missing_dependencies='|'.join(sorted(missing)),
                           uninterpreted_dependencies='|'.join(sorted(unread)),
                           unsupported_mechanisms='|'.join(sorted(unsupported)),
                           decoded='no_visual_declared' if not ids else ('partial' if missing or unread else 'core_records_decoded'),
                           implementation='no_visual_declared' if not ids else 'partial_shared_mechanisms',
                           structural_tests='shared_mechanisms_only:SK2.9_paths;SK2.10_curves_models;SK2.11_geometry_and_weapon_hands;SK2.12_camera_parent_ground_and_stop',
                           cpu_execution='no_channels' if not ids else ('batches_generated' if num(p.get('nonempty_samples','0')) else 'no_batches_at_sampled_times'),
                           runtime_gl=evidence.get(str(sid),{}).get('status','not_validated_per_skill'),
                           runtime_evidence='|'.join(evidence.get(str(sid),{}).get('files',[])), visual_against_dx9=evidence.get(str(sid),{}).get('visual_status','pending'),
                           notes='CPU samples at 0,.1,.35,.7,1.1,2s; water+target+ground. Geometry generation is not visual fidelity.'))
    write_csv(out / 'SKILLS_MIGRATION_STATUS.csv', list(result[0]), result)
    manifest = [dict(file=rel, bytes=path.stat().st_size, sha256=hashlib.sha256(path.read_bytes()).hexdigest())
                for rel, path in sorted(assets.items()) if path.suffix.lower() in ('.eff','.par')]
    write_csv(out / 'ASSET_MANIFEST_SHA256.csv', ['file','bytes','sha256'], manifest)
    par_summary = json.loads((out / 'par/par_summary.json').read_text())
    blend_summary = json.loads((out / 'blend/blend_summary.json').read_text())
    summary = dict(schema='SK2.12-coverage-v1', skill_records=len(result),
                   skills_with_visual_channels=sum(bool(r['resources_required']) for r in result),
                   skills_with_missing_dependencies=sum(bool(r['missing_dependencies']) for r in result),
                   skills_with_uninterpreted_dependencies=sum(bool(r['uninterpreted_dependencies']) for r in result),
                   eff_files=blend_summary['eff_files'], eff_decode_failures=len(failed_eff),
                   par_files=par_summary['files_scanned'], systems=par_summary['systems_decoded'],
                   direct_controller_strips=len(direct_strips),
                   direct_character_tracks=sum(v[1] for v in controller_tails.values()),
                   unexplained_par_tail_files=sum(v[2]!=0 for v in controller_tails.values()),
                   particle_types=par_summary['particle_types'], historical_blend_pairs=blend_summary['distinct_pairs'],
                   tables_sha256={name:hashlib.sha256((client/'scripts/table'/name).read_bytes()).hexdigest()
                                  for name in ('skillinfo.txt','sceneffectinfo.txt','magicsingleinfo.txt','magicgroupinfo.txt')},
                   magic_modes=dict(sorted(collections.Counter(num(m[8]) for m in magic.values()).items())),
                   skills_by_mode=dict(sorted(skill_mode_counts.items())),
                   notes=['Core PAR reader leaves controller tails on 1021 files; coverage separately inspects CMPStrip/CChaModel tails.',
                          'Legacy blend implementation/status labels are stale; only raw factors/counts used.',
                          'Coverage does not certify full LGO decoding, material textures or animated LAB skeletons.',
                          'No skill is classified as visually validated against DX9.'])
    mechanisms=[]
    for mode,name in enumerate(MODES):
        mechanisms.append(dict(category='movement',mechanism=f'{mode}:{name}',resource_count=summary['magic_modes'].get(mode,0),
                               implementation='stationary_target_path_or_cycle_implemented',
                               tests='UI05SK29Tests.cpp',visual_validation='pending',
                               remaining={0:'terrain_collision',1:'no_current_table_entries',2:'moving_target_tracking',
                                          3:'terrain_collision;frame_order;3D_copies;16_copy_cap',4:'terrain_collision',
                                          5:'particle_binding_review',6:'authored_range_override'}[mode]))
    for t,name in TYPES.items():
        state = 'MODEL_STRIP_core_and_RectPlane' if t in (5,6) else (
            'RIPPLE_stationary_position_native_and_eff_geometry_partial' if t==4 else
            'SHADE_ground_controller_pending' if t==13 else 'nested_eff_geometry_with_approximate_kinematics')
        mechanisms.append(dict(category='particle',mechanism=f'{t}:{name}',resource_count=par_summary['particle_types'].get(str(t),0),
                               implementation=state,
                               tests='UI05SK210Tests.cpp;UI05SK211Tests.cpp' if t in (5,6) else 'UI05SK211Tests.cpp;CPU_smoke',visual_validation='pending',
                               remaining='actor_bone_world_matrices;full_emission_lifecycle' if t in (5,6) else 'historical_kinematics_emission_and_model_geometry'))
    for mechanism,count,state,pending in [
        ('PAR_curves',par_summary['systems_decoded'],'N_intervals_and_MODEL_STRIP_absolute_stop_implemented','per_particle_birth_and_life'),
        ('EFF_timeline',blend_summary['eff_files'],'durations_and_controller_cycles','31_undecoded_files;zero_duration_single_key_exception'),
        ('direct_CMPStrip',len(direct_strips),'decoded_by_coverage_only','dummy_history_geometry_renderer'),
        ('blend_pairs',blend_summary['distinct_pairs'],'SK2.6_mapper_default_on_RGB_backbuffer','per_effect_visual_comparison_against_DX9'),
        ('dummies_and_attachments',None,'weapon_hands_during_actions_internal_matrices','effect_world_matrix_bridge_and_effect_identity'),
        ('animation_events',None,'self_cast_begin_key_end_results_and_existing_flight_schedule','per_item_callbacks_and_arrival_damage'),
        ('billboard_camera',None,'inverse_view_right_and_up_implemented','per_effect_visual_comparison'),
        ('nested_model_transform',None,'parent_scale_rotation_translation_and_D3DX_order_implemented','bone_binding;billboard_parent_rotation_review'),
        ('surface_anchor',None,'ground_water_separate_from_target_body_height','waterline_and_bridge_surface_review'),
        ('render_states',None,'existing_renderer','ztest_zwrite_cull_sampler_order_billboard')]:
        mechanisms.append(dict(category='shared',mechanism=mechanism,resource_count=count,implementation=state,
                               tests='see_test_logs',visual_validation='pending',remaining=pending))
    write_csv(out/'MECHANISMS_STATUS.csv',list(mechanisms[0]),mechanisms)
    (out / 'SUMMARY.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2)+'\n')
    print(json.dumps({k:summary[k] for k in ('skill_records','eff_files','par_files','systems','eff_decode_failures')}, indent=2))


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--client', default='Client')
    p.add_argument('--build', default='build')
    p.add_argument('--out', default='deliverables/SK2.12/coverage')
    p.add_argument('--runtime-evidence', help='Optional JSON of partial GL captures; referenced files must exist')
    a = p.parse_args()
    audit(a.client, a.build, a.out, a.runtime_evidence)
