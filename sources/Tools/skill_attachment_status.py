#!/usr/bin/env python3
"""Join current skill tables, decoded dependencies and measured preview evidence.
No row asserts complete implementation, combat execution or DX9 visual parity.
"""
import argparse
import collections
import csv
import json
from pathlib import Path


def rows(path):
    return list(csv.DictReader(Path(path).open(newline='', encoding='latin1')))


def table(path):
    return {int(c[0]): c for line in Path(path).read_bytes().decode('latin1').splitlines()
            if (c := line.split('\t'))[0].isdigit() and int(c[0]) > 0}


def number(value):
    try:
        return int(value)
    except ValueError:
        return 0


def names(value):
    return [s.strip() for s in value.split(',') if s.strip() not in ('', '0')]


def run(client, audit, comparison):
    client, audit, comparison = map(Path, (client, audit, comparison))
    skills = table(client/'scripts/table/skillinfo.txt')
    magic = table(client/'scripts/table/magicsingleinfo.txt')
    group = table(client/'scripts/table/magicgroupinfo.txt')
    scene = table(client/'scripts/table/sceneffectinfo.txt')
    graph = collections.defaultdict(set)
    missing = collections.defaultdict(set)
    types = collections.defaultdict(set)
    for r in rows(audit/'par/par_systems.csv'):
        types[r['file']].add(r['type'])
    for r in rows(audit/'par/par_dependencies.csv'):
        if r['status'] != 'found':
            missing[r['file']].add(r['kind']+':'+r['declared_name'])
        if r['resolved_path'] and r['resolved_path'].lower().endswith(('.par', '.eff', '.lgo')):
            path = Path(r['resolved_path'])
            # Auditor paths may be absolute or relative to the project.
            if 'effect' in path.parts:
                suffix = path.as_posix().split('/effect/', 1)[-1]
                graph[r['file']].add('model/effect/'+suffix if '/model/effect/' in path.as_posix()
                                     else 'effect/'+suffix)
    primitives = {'Triangle', 'TrianglePlane', 'Rect', 'RectPlane', 'RectZ', 'Cone', 'Cylinder'}
    for r in rows(audit/'blend/blend_usage.csv'):
        if r['kind'] != 'eff' or not r['model'] or r['model'] in primitives:
            continue
        graph[r['file']].add('model/effect/'+r['model']+
                             ('' if r['model'].endswith('.lgo') else '.lgo'))
    units = collections.defaultdict(set)
    for r in rows(audit/'EFFECT_MODEL_UNITS.csv'):
        if r['status'] != 'decoded_key_bounds':
            units['effect/'+r['file']].add(r['status'])
    gaps = {r['skill_id']: r['gaps'] for r in rows(audit/'coverage/fx_skill_gaps.csv')}
    probe = {r['skill_id']: r for r in rows(audit/'cpu_probe.csv')}
    captures = collections.defaultdict(list)
    measured = json.loads((comparison/'after/runs.json').read_text())
    for r in measured['runs']:
        if r['exit_code'] == 0 and r['shot_exists']:
            captures[r['skill']].append(f"{r['channel']}@{r['seconds']}s")
    output = []
    for sid, c in sorted(skills.items()):
        required, modes, unresolved = set(), set(), set()
        def effect(visual):
            if 2000 <= visual < 3000:
                row = group.get(visual-2000)
                if row:
                    for mid in names(row[3]):
                        effect(1000+number(mid))
                else:
                    unresolved.add('group:'+str(visual))
            elif 1000 <= visual < 2000:
                row = magic.get(visual-1000)
                if not row:
                    unresolved.add('magic:'+str(visual)); return
                modes.add(row[8])
                required.update('effect/'+s for s in names(row[3]))
                required.update('effect/'+s+('' if s.endswith('.par') else '.par')
                                for s in names(row[6])+names(row[10]))
            elif visual > 0:
                row = scene.get(visual)
                if row:
                    required.add('effect/'+row[1])
                else:
                    unresolved.add('scene:'+str(visual))
        for raw in names(c[51]):
            effect(number(raw))
        for index in (54, 55, 59, 63, 65, 66):
            effect(number(c[index].split(',')[0]))
        pending = list(required)
        while pending:
            source = pending.pop()
            for child in graph[source]-required:
                required.add(child); pending.append(child)
        absent = {'file:'+p for p in required if not (client/p).is_file()}
        for p in required:
            absent.update(missing[p])
        unit_gaps = {p+':'+v for p in required for v in units[p]}
        p = probe[str(sid)]
        output.append(dict(skill_id=sid, name=c[1], resources_required='|'.join(sorted(required)),
            movement_modes='|'.join(sorted(modes)),
            particle_types='|'.join(sorted(set().union(*(types[x] for x in required)))),
            missing_dependencies='|'.join(sorted(absent)),
            unresolved_table_ids='|'.join(sorted(unresolved)),
            model_unit_audit_gaps='|'.join(sorted(unit_gaps)),
            known_mechanism_gaps=gaps.get(str(sid), ''),
            implementation='partial_shared_mechanisms;SK2.19_owner_chain_and_authored_units',
            structural_test='CPU_probe_nonfinite='+p['nonfinite_vertices']+';nonempty_samples='+p['nonempty_samples'],
            runtime_preview='|'.join(captures[sid]) or 'not_captured',
            combat_execution='not_validated_in_this_migration',
            original_visual_comparison='qualitative_partial_user_PNG' if sid in (81,84) else 'pending',
            fully_faithful='no'))
    with (audit/'SKILLS_MIGRATION_STATUS.csv').open('w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=list(output[0])); writer.writeheader(); writer.writerows(output)
    print('Current skills:', len(output), 'captured:', sum(bool(value) for value in captures.values()), 'none certified fully faithful')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client', type=Path, default=Path('Client'))
    parser.add_argument('--audit', type=Path, required=True)
    parser.add_argument('--comparison', type=Path, required=True)
    args = parser.parse_args()
    run(args.client, args.audit, args.comparison)
