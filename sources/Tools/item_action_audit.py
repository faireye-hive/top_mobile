#!/usr/bin/env python3
"""05-AW: Audita TODOS os registros historicos (sem executar Lua).
Uso: python3 sources/Tools/item_action_audit.py Client/scripts /tmp/item-audit.csv /tmp/item-audit.md
Usa apenas a biblioteca padrao. Os dados do cliente NAO sao incorporados ao ZIP sources/.
"""
import csv
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path


def read_latin(path):
    return Path(path).read_bytes().decode('latin1')


def random_reward_tables(root):
    """Return item IDs whose literal GiveRandomItems table is readable."""
    base = Path(root)
    effect = base/'offline/script/calculate/ItemEffect.lua'
    variables = base/'offline/calculate/variable.lua'
    if not effect.is_file() or not variables.is_file():
        return set()
    script = read_latin(effect)
    match = re.search(r'function\s+GiveRandomItems\s*\([^\n]*\)([\s\S]*?)(?=\n\s*function\s+|\s*$)', script)
    if not match:
        return set()
    item_tables = {int(item): table for item, table in
                   re.findall(r'rItem\s*\[\s*(\d+)\s*\]\s*=\s*([A-Za-z_]\w*)', match.group(1))}
    required = set(item_tables.values())
    found = set()
    for table, _index, body in re.findall(
            r'([A-Za-z_]\w*)\s*\[\s*(\d+)\s*\]\s*=\s*\{([^}]*)\}', read_latin(variables)):
        if table not in required:
            continue
        fields = {key: int(value) for key, value in
                  re.findall(r'(Active|ItemID|Quantity|Quality|Rad|GoodItem)\s*=\s*(-?\d+)', body)}
        if (fields.get('Active') == 1 and fields.get('ItemID', 0) > 0 and
                fields.get('Quantity', 0) > 0 and fields.get('Quality', -1) >= 0 and
                fields.get('Rad', 0) > 0 and fields.get('GoodItem') in (0, 1)):
            found.add(table)
    return {item for item, table in item_tables.items() if table in found}


def black_market_tables(root):
    """Return item IDs mapped by BlackMarket to readable weighted tables."""
    base = Path(root)
    effect = base/'offline/script/calculate/ItemEffect.lua'
    variables = base/'offline/calculate/variable.lua'
    if not effect.is_file() or not variables.is_file():
        return set()
    script = read_latin(effect)
    match = re.search(r'function\s+BlackMarket\s*\([^\n]*\)([\s\S]*?)(?=\n\s*function\s+|\s*$)', script)
    if not match:
        return set()
    item_tables = {int(item): table for item, table in
                   re.findall(r'Market\s*\[\s*(\d+)\s*\]\s*=\s*([A-Za-z_]\w*)', match.group(1))}
    required = set(item_tables.values())
    found = set()
    for table, _index, body in re.findall(
            r'([A-Za-z_]\w*)\s*\[\s*(\d+)\s*\]\s*=\s*\{([^}]*)\}', read_latin(variables)):
        if table not in required:
            continue
        fields = {key: int(value) for key, value in
                  re.findall(r'(Active|ItemID|Quantity|Quality|Rad|GoodItem)\s*=\s*(-?\d+)', body)}
        if (fields.get('Active') == 1 and fields.get('ItemID', 0) > 0 and
                fields.get('Quantity', 0) > 0 and fields.get('Quality', -1) >= 0 and
                fields.get('Rad', 0) > 0 and fields.get('GoodItem') in (0, 1)):
            found.add(table)
    return {item for item, table in item_tables.items() if table in found}


def corsairs_chest_items(root):
    """IDs backed by literal historical Corsairs equipment/weapon chest rows."""
    effect = Path(root)/'offline/script/calculate/ItemEffect.lua'
    if not effect.is_file():
        return {}
    script = read_latin(effect)
    mapped = {}
    for callback in ('Corsairs_EquipmentChest', 'Corsairs_WeaponChest', 'Corsairs_WeaponChest2'):
        match = re.search(r'function\s+' + re.escape(callback) +
                          r'\s*\([^\n]*\)([\s\S]*?)(?=\n\s*function\s+|\s*$)', script)
        if not match:
            continue
        for item, body in re.findall(r'Chest\s*\[\s*(\d+)\s*\]\s*=\s*\{([^\n]*)', match.group(1)):
            active = re.search(r'Active\s*=\s*(\d+)', body)
            if active and active.group(1) == '1' and re.search(r'Item\s*=', body):
                mapped[int(item)] = callback
    return mapped


def item_stat_ids(root):
    variable = Path(root)/'offline/calculate/variable.lua'
    if not variable.is_file():
        return set()
    data = read_latin(variable)
    return {int(item) for item in re.findall(
        r'Server\.Restore\.Attr\s*\[\s*(\d+)\s*\]\s*=\s*\{\s*Name\s*=\s*"[^"]+"\s*,\s*Amount\s*=\s*-?\d+\s*,\s*Attribute\s*=\s*ATTR_B(?:STR|CON|AGI|DEX|STA)\s*\}', data)}


def audit(root, csv_output, md_output):
    table = Path(root)/'table/iteminfo.txt'
    var_file = Path(root)/'offline/calculate/variable.lua'
    rows = read_latin(table).splitlines()
    lua = read_latin(var_file)
    restore = {}
    for match in re.finditer(r'Restore\.Life\[\s*(\d+)\s*\]\s*=\s*\{([^\n]*)', lua):
        body = match.group(2)
        amount = re.search(r'Amount\s*=\s*\{HP\s*=\s*([\d.]+),\s*SP\s*=\s*([\d.]+)\}', body)
        percentage = re.search(r'Percentage\s*=\s*\{HP\s*=\s*([\d.]+),\s*SP\s*=\s*([\d.]+)\}', body)
        state = re.search(r'State\s*=\s*([^,}]+)', body)
        state_id = int(state.group(1)) if state and state.group(1).strip().isdigit() else 0
        restore[int(match.group(1))] = {
            'amount': amount.groups() if amount else ('0', '0'),
            'percentage': percentage.groups() if percentage else ('0', '0'),
            'state': state.group(1).strip() if state else 'nil',
            'state_id': state_id,
        }
    book_map = {int(n) for n in re.findall(r'Server\.Player\.Skill\[\s*(\d+)\s*\]\s*=\s*\{\s*Skill\s*=', lua)}
    skill_names = defaultdict(set)
    for skill_row in read_latin(Path(root)/'table/skillinfo.txt').splitlines():
        cols = skill_row.split('\t')
        if len(cols) > 1 and cols[0].strip().isdigit():
            skill_names[cols[1].strip().lower()].add(int(cols[0]))
    exact_skill_name_items = set()
    for item_row in rows:
        cols = item_row.split('\t')
        if len(cols) > 86 and cols[0].strip().isdigit() and cols[86].strip() == 'AddPlayerSkill':
            if len(skill_names.get(cols[1].strip().lower(), set())) == 1:
                exact_skill_name_items.add(int(cols[0]))
    life_book_map = {int(n) for n in re.findall(
        r'Server\.Life\.Skill\[\s*(\d+)\s*\]\s*=\s*\{\s*Skill\s*=',
        read_latin(Path(root)/'offline/script/calculate/variable.lua'))}
    ticket_rules = {int(n) for n in re.findall(
        r'TicketSys\.Ticket\[\s*(\d+)\s*\]\s*=',
        read_latin(Path(root)/'offline/script/calculate/variable.lua'))}
    ticket_impl = Path(__file__).resolve().parents[1]/'Engine/src/item/ItemTicketRules.cpp'
    ticket_destinations = {int(n) for n in re.findall(
        r'case\s+(\d+)\s*:\s*return Destination', read_latin(ticket_impl))}
    fairy_rations = {int(item): int(amount) for item, amount in re.findall(
        r'Server\.Fairy\.Ration\[\s*(\d+)\s*\]\s*=\s*(\d+)',
        read_latin(Path(root)/'offline/script/calculate/variable.lua'))}
    fairy_auto_rations = {int(item) for item in re.findall(
        r'Server\.Fairy\.AutoFeed\.(?:Normal|Great)\s*=\s*(\d+)',
        read_latin(Path(root)/'offline/script/calculate/variable.lua'))}
    fairy_skill_books = {int(item) for item in re.findall(
        r'Server\.Fairy\.Skill\[\s*(\d+)\s*\]\s*=\s*\{\s*Skill\s*=\s*\d+\s*,\s*Level\s*=\s*\d+',
        read_latin(Path(root)/'offline/script/calculate/variable.lua'))}
    fairy_eggs = {int(egg): int(fairy) for egg, fairy in re.findall(
        r'Server\.Fairy\.Egg\[\s*(\d+)\s*\]\s*=\s*\{\s*ID\s*=\s*(\d+)',
        read_latin(Path(root)/'offline/script/calculate/variable.lua'))}
    fairy_type_ids = set()
    for row in rows:
        cols = row.split('\t')
        if len(cols) >= 11 and cols[0].strip().isdigit() and cols[10].strip() == '59':
            fairy_type_ids.add(int(cols[0]))
    random_reward_items = random_reward_tables(root)
    black_market_items = black_market_tables(root)
    corsairs_chests = corsairs_chest_items(root)
    fixed_set_chests = {
        2836: 'ItemUse_Hadisi', 2837: 'ItemUse_Anhei', 2838: 'ItemUse_Diyu',
        2839: 'ItemUse_Xiuluo', 2840: 'ItemUse_Youming', 2841: 'ItemUse_Minghe',
    }
    fixed_historical_chests = {
        192: 'ItemUse_SaintCloth',
        209: 'ItemUse_SCBoxYXTZ', 210: 'ItemUse_SCBoxLSTZ',
        211: 'ItemUse_SCBoxHYTZ', 212: 'ItemUse_SCBoxFYSTZ',
        213: 'ItemUse_SCBoxSZZTZ', 214: 'ItemUse_SCBoxHHSTZ',
        215: 'ItemUse_SCBoxRYBZ', 216: 'ItemUse_SCBoxXMC',
        217: 'ItemUse_SCBoxLQJ', 218: 'ItemUse_SCBoxLYJ',
        219: 'ItemUse_SCBoxFHG', 220: 'ItemUse_SCBoxLXHZ',
        221: 'ItemUse_SCBoxYCJ',
    }
    stat_items = item_stat_ids(root)
    records = []
    by_status = Counter()
    by_callback = Counter()
    by_type = Counter()
    unresolved = defaultdict(list)
    for row in rows:
        cols = row.split('\t')
        if len(cols) < 87 or not cols[0].strip().isdigit():
            continue
        item_id = int(cols[0])
        cb = cols[86].strip()
        typ = cols[10].strip()
        slots = [int(p) for p in re.findall(r'-?\d+', cols[28])]
        equip = any(slot >= 0 for slot in slots)
        # Nao assumir que funcao Lua desconhecida possui efeito implementado.
        if cb == 'ItemMedKit':
            rule = restore.get(item_id)
            if not rule:
                status = 'heal_no_formula'
            elif rule['state_id']:
                status = 'heal_state_supported' if rule['state_id'] in (165, 166, 167) else 'heal_state_PORT_PARTIAL'
            elif any(float(value) > 0 for value in rule['percentage']):
                status = 'heal_percentage_supported'
            elif any(int(value) > 0 for value in rule['amount']):
                status = 'heal_amount_supported'
            else:
                status = 'heal_no_formula'
        elif cb == 'ItemUse_kala':
            status = 'kala_stamina_state_D1' if item_id == 1021 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_Fish':
            status = 'boat_food_SP_PORT_APPROX' if item_id == 4551 and typ == '41' else 'lua_callback_not_implemented'
        elif cb == 'SummonAries':
            status = 'aries_guardian_D1' if item_id == 2962 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_NSSN':
            status = 'goddess_guardian_D1' if item_id == 1014 and typ == '31' else 'lua_callback_not_implemented'
        elif cb in ('ItemUse_HJZHQ', 'ItemUse_MLZHQ', 'ItemUse_XRZHQ', 'ItemUse_SDZHQ'):
            holiday_pets = {2888: 'ItemUse_HJZHQ', 2889: 'ItemUse_MLZHQ',
                            2890: 'ItemUse_XRZHQ', 2891: 'ItemUse_SDZHQ'}
            status = 'holiday_pet_summon_PORT_APPROX' if holiday_pets.get(item_id) == cb and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_MHDYSD':
            status = 'medical_report_summon_D1' if item_id == 1027 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_BYSHJZ':
            status = 'white_sheep_guardian_summon_PORT_APPROX' if item_id == 2990 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_minibh':
            status = 'mini_black_dragon_bag_PORT_APPROX' if item_id == 1019 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_TunDuiZhiXing':
            status = 'star_of_unity_chest_D1' if item_id == 1033 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_YingbiBox':
            status = 'fairy_coin_chest_PORT_APPROX' if item_id == 1872 and typ == '41' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_SuanmingMoney':
            status = 'fortune_lot_PORT_APPROX' if item_id == 3087 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_SuanmingWork':
            status = 'career_lot_PORT_APPROX' if item_id == 3086 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_Zhenheilong':
            status = 'black_dragon_rightful_chest_PORT_APPROX' if item_id == 2843 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_HQBOX':
            status = 'wedding_gift_parcel_D1' if item_id == 2907 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_XYPIGBOX':
            status = 'lucky_piggy_chest_D1' if item_id == 2909 and typ == '31' else 'lua_callback_not_implemented'
        elif cb in ('ItemUse_CZBox', 'ItemUse_JNCZBox'):
            apparel_chests = {2955: 'ItemUse_CZBox', 3035: 'ItemUse_JNCZBox'}
            status = 'fixed_apparel_chest_PORT_APPROX' if apparel_chests.get(item_id) == cb and typ == '31' else 'lua_callback_not_implemented'
        elif cb in ('ItemUse_XYZSTL', 'ItemUse_HDCZ', 'ItemUse_HY', 'ItemUse_HLHT', 'ItemUse_JNSHZ'):
            fixed_summons = {3012: 'ItemUse_XYZSTL', 3014: 'ItemUse_HDCZ',
                             3015: 'ItemUse_HY', 3016: 'ItemUse_HLHT', 3036: 'ItemUse_JNSHZ'}
            status = 'fixed_summon_scroll_PORT_APPROX' if fixed_summons.get(item_id) == cb and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'InventorySlot' and 3088 <= item_id <= 3093:
            status = 'bag_upgrade_supported'
        elif cb == 'AddPlayerSkill':
            status = ('skill_book_mapped' if item_id in book_map else
                      'skill_book_name_match_PORT_APPROX' if item_id in exact_skill_name_items else
                      'skill_book_no_mapping')
        elif cb == 'AddLifeSkill':
            status = 'life_skill_book_mapped' if item_id in life_book_map else 'lua_callback_not_implemented'
        elif cb == 'Ticket':
            if item_id in ticket_rules and item_id in ticket_destinations:
                status = 'ticket_destination_PORT_APPROX'
            elif item_id in ticket_rules:
                status = 'ticket_no_destination'
            else:
                status = 'lua_callback_not_implemented'
        elif cb == 'FairyRation':
            status = ('fairy_ration_auto_PORT_APPROX' if item_id in fairy_auto_rations else
                      'fairy_ration_supported' if item_id in fairy_rations else
                      'lua_callback_not_implemented')
        elif cb == 'ItemUse_Egg':
            status = ('fairy_egg_PORT_APPROX' if item_id in fairy_eggs and
                      fairy_eggs[item_id] in fairy_type_ids else 'lua_callback_not_implemented')
        elif cb == 'AddFairySkill':
            status = 'fairy_skill_book_supported' if item_id in fairy_skill_books and typ == '58' else 'lua_callback_not_implemented'
        elif cb == 'AddFairyBody':
            status = 'fairy_possession_book_D1' if item_id in {239, 608, 609, 610, 611, 612} and typ == '34' else 'lua_callback_not_implemented'
        elif cb == 'AddFunnySkill':
            status = 'fairy_funny_book_D1' if item_id in {6456, 6457, 6458, 6459, 6460, 6461, 6462, 6463} and typ == '34' else 'lua_callback_not_implemented'
        elif cb == 'ExpCheckSys_Toggle':
            status = 'monster_exp_toggle_D1' if item_id == 9621 and typ == '36' else 'lua_callback_not_implemented'
        elif cb == 'Book_SkillReset':
            status = 'combat_skill_reset_D1' if item_id == 996 and typ == '34' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_XiDianBook':
            status = 'skill_reset_book_PORT_PARTIAL' if item_id == 2216 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_PKSFYS':
            status = 'harden_potion_D1' if item_id == 1858 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_DSSDM':
            status = 'lost_christmas_cap_D1' if item_id == 3888 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_XiongHJ':
            status = 'festive_wine_D1' if item_id == 264 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_IDBOX':
            status = 'ended_event_box_D1' if item_id == 24 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_Admiral':
            status = 'admiral_cloak_chest_PORT_APPROX' if item_id == 9603 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_AiCao':
            status = 'ether_clover_D1' if item_id == 265 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_Crbbt':
            status = 'super_lollipop_D1' if item_id == 584 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_Jrqkl':
            status = 'huge_chocolate_D1' if item_id == 585 and typ == '31' else 'lua_callback_not_implemented'
        elif cb in ('ItemUse_SPXsyj', 'ItemUse_SPLhyj', 'ItemUse_SPMnyj', 'ItemUse_SPYyyj', 'ItemUse_SPSlyj',
                    'ItemUse_SPXsyjA', 'ItemUse_SPLhyjA', 'ItemUse_SPMnyjA', 'ItemUse_SPYyyjA', 'ItemUse_SPSlyjA'):
            potion_ids = {'ItemUse_SPXsyj': 897, 'ItemUse_SPLhyj': 893, 'ItemUse_SPMnyj': 895,
                          'ItemUse_SPYyyj': 894, 'ItemUse_SPSlyj': 896, 'ItemUse_SPXsyjA': 3100,
                          'ItemUse_SPLhyjA': 3101, 'ItemUse_SPMnyjA': 3102, 'ItemUse_SPYyyjA': 3103,
                          'ItemUse_SPSlyjA': 3104}
            status = 'attribute_potion_D1' if item_id == potion_ids[cb] and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_MspdYSB':
            status = 'weightless_potion_D1' if item_id == 850 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_XEZJB':
            status = 'battle_array_D1' if item_id == 851 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_DenglongB':
            status = 'full_body_armor_D1' if item_id == 852 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_DenglongA':
            status = 'lantern_defense_D1' if item_id == 3107 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_Denglong':
            status = 'lantern_defense_D1' if item_id == 3342 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_SanJiaoFan':
            status = 'boat_speed_sail_D1' if item_id in (3046, 3340) and typ == '31' else 'lua_callback_not_implemented'
        elif cb in ('ItemUse_WQZZ', 'ItemUse_DHZ', 'ItemUse_DSZ', 'ItemUse_NMZ'):
            rusty_dumplings = {'ItemUse_WQZZ': 3037, 'ItemUse_DHZ': 3043,
                               'ItemUse_DSZ': 3044, 'ItemUse_NMZ': 3045}
            status = ('expired_dumpling_D1' if typ == '31' and rusty_dumplings[cb] == item_id
                      else 'lua_callback_not_implemented')
        elif cb == 'ItemUse_LSDZG':
            status = 'forge_catalyst_D1' if item_id in (3074, 3883) and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_HSDZG':
            status = 'combine_catalyst_D1' if item_id in (3075, 3884) and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_PartyFruit':
            status = 'party_exp_fruit_PORT_APPROX' if item_id in (849, 6019) and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_wxlh':
            status = 'reincarnation_pagoda_D1' if item_id == 240 and typ == '31' else 'lua_callback_not_implemented'
        elif cb in {'ItemUse_lieyanBook','ItemUse_zhiyanBook','ItemUse_huoyaoBook','ItemUse_manaoBook',
                    'ItemUse_shengguangBook','ItemUse_hanyuBook','ItemUse_yuezhixinBook','ItemUse_xianlingBook',
                    'ItemUse_fenglingBook','ItemUse_yingyanBook','ItemUse_yanyuBook','ItemUse_tanyuBook'}:
            fixed_gem_vouchers = {
                'ItemUse_lieyanBook': (856,878), 'ItemUse_zhiyanBook': (857,879),
                'ItemUse_huoyaoBook': (858,880), 'ItemUse_manaoBook': (859,881),
                'ItemUse_shengguangBook': (911,887), 'ItemUse_hanyuBook': (914,882),
                'ItemUse_yuezhixinBook': (915,883), 'ItemUse_xianlingBook': (916,884),
                'ItemUse_fenglingBook': (918,860), 'ItemUse_yingyanBook': (919,861),
                'ItemUse_yanyuBook': (920,862), 'ItemUse_tanyuBook': (921,863),
            }
            status = ('fixed_gem_voucher_D1' if typ == '31' and
                      fixed_gem_vouchers[cb][0] == item_id else 'lua_callback_not_implemented')
        elif cb == 'ItemUse_tedengjiang':
            status = 'black_dragon_grand_prize_D1' if item_id == 272 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_yidengjiang':
            status = 'black_dragon_first_prize_D1' if item_id == 273 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_erdengjiang':
            status = 'black_dragon_second_prize_PORT_PARTIAL' if item_id == 274 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_sandengjiang':
            status = 'black_dragon_third_prize_D1' if item_id == 275 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_SANWUBOX':
            status = 'ultimate_chest_D1' if item_id == 985 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_JaNaBox':
            status = 'ascaron_chest_D1' if item_id == 1017 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_SDDLB':
            status = 'christmas_gift_D1' if item_id == 2897 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_GWZHQ':
            status = 'posh_christmas_box_D1' if item_id == 2900 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_SZF':
            status = 'number_ticket_PORT_APPROX' if item_id == 2305 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_XNBOX':
            status = 'new_year_chest_PORT_APPROX' if item_id == 2307 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_HHLP':
            status = 'grandiose_salute_PORT_APPROX' if item_id == 1011 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_FightingBox':
            status = 'chaos_chest_D1' if item_id == 1134 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_PKJSYS':
            status = 'energy_potion_D1' if item_id == 1857 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_MspdYS':
            status = 'acceleration_potion_D1' if item_id == 3339 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_MspdYSA':
            status = 'skating_potion_D1' if item_id == 3105 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_PKZDYS':
            status = 'battle_potion_D1' if item_id == 1855 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_PKKBYS':
            status = 'berserk_potion_PORT_APPROX' if item_id == 1856 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_PKJZYS':
            status = 'accurate_potion_D1' if item_id == 1859 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_PKMNYS':
            status = 'bull_potion_D1' if item_id == 1854 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_SDDC':
            status = 'christmas_dinner_D1' if item_id == 2898 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_NiceCake':
            status = 'famous_cake_D1' if item_id == 2988 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == '0444':
            status = 'blessed_potion_D1' if item_id == 1860 and typ == '71' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_MWHJ':
            status = 'tasty_turkey_exp_D1' if item_id == 2895 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_YB':
            status = 'mooncake_exp_D1' if item_id == 3915 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_SIWUBOX':
            status = 'ultimate_chest_D1' if item_id == 986 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_WUWUBOX':
            status = 'ultimate_chest_D1' if item_id == 987 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_LIUWUBOX':
            status = 'ultimate_chest_D1' if item_id == 988 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_65BBBox':
            status = 'class_boss_box_PORT_APPROX' if item_id == 241 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_JingLingBOX':
            status = 'fairy_box_PORT_APPROX' if item_id == 262 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_JNJLD':
            status = 'fairy_mystery_egg_PORT_APPROX' if item_id == 1015 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_NSDXB':
            status = 'goddess_pouch_PORT_APPROX' if item_id == 1016 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_ALDXB':
            status = 'aladdin_parcel_PORT_PARTIAL' if item_id == 2371 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_CZZX':
            status = 'innocence_heart_D1' if item_id in (1009, 1013) and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_MonsterSummon':
            status = 'monster_summon_D1' if item_id == 319 and typ == '31' else 'lua_callback_not_implemented'
        elif cb in ('FairyFruit', 'FairyFruitImprvd'):
            status = ('fairy_fruit_PORT_PARTIAL' if item_id in
                      {222,223,224,225,226,276,277,278,279,280,7003,7004,7005,7006,7007} and typ == '58'
                      else 'lua_callback_not_implemented')
        elif cb == 'Blueprint':
            status = 'blueprint_generator_PORT_APPROX' if item_id in {1000, 1001, 1002, 1003, 8909} else 'lua_callback_not_implemented'
        elif cb == 'FairySkillRemoval':
            status = 'fairy_skill_removal_D1' if item_id in (9609, 9610, 9611) and typ == '34' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_BZ':
            status = 'historical_noop_item_callback_D1'
        elif cb == 'ItemUse_BirthCake':
            status = 'birth_cake_PORT_APPROX' if item_id in (3240, 3338) else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_CZHe':
            status = 'gold_note_D1' if item_id == 854 else 'lua_callback_not_implemented'
        elif cb == 'Cloak_Upgrade':
            status = 'admiral_cloak_upgrade_D1' if item_id == 9605 and typ == '66' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_HappyBook':
            status = 'happy_book_supported' if item_id in (853, 2989) else 'lua_callback_not_implemented'
        elif cb in {'ItemUse_DathBagA','ItemUse_DathBagB','ItemUse_DathBagC','ItemUse_DathBagD',
                    'ItemUse_DathBagE','ItemUse_DathBagF','ItemUse_DathBagG','ItemUse_DathBagH'}:
            death_burdens = {'ItemUse_DathBagA': 2919, 'ItemUse_DathBagB': 2920,
                             'ItemUse_DathBagC': 2921, 'ItemUse_DathBagD': 2922,
                             'ItemUse_DathBagE': 2923, 'ItemUse_DathBagF': 2924,
                             'ItemUse_DathBagG': 2925, 'ItemUse_DathBagH': 2926}
            status = 'death_burden_D1' if item_id == death_burdens[cb] and typ == '31' else 'lua_callback_not_implemented'
        elif cb in fixed_set_chests.values():
            status = 'fixed_boss_set_chest_supported' if fixed_set_chests.get(item_id) == cb else 'lua_callback_not_implemented'
        elif cb in fixed_historical_chests.values():
            status = ('fixed_historical_chest_D1' if fixed_historical_chests.get(item_id) == cb and typ == '41'
                      else 'lua_callback_not_implemented')
        elif cb == 'ItemUse_Sishen':
            status = 'class_boss_weapon_chest_PORT_APPROX' if item_id == 2842 else 'lua_callback_not_implemented'
        elif cb == 'Book_StatReset':
            status = 'stat_reset_supported' if item_id == 997 else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_Foolish':
            status = 'foolish_exp_supported' if item_id == 2953 else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_CZKC':
            status = 'growth_fast_food_exp_supported' if item_id == 992 else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_XingYunBox':
            status = 'lucky_card_chest_PORT_APPROX' if item_id == 3011 else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_MoHuanBox':
            status = 'enchanting_card_chest_PORT_APPROX' if item_id == 1035 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_mohuan':
            status = 'enchanting_goddess_card_PORT_APPROX' if item_id == 1036 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_HYBOX':
            status = 'lucky_packet_PORT_APPROX' if item_id == 3901 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_MarchElf':
            status = 'fairy_march_PORT_APPROX' if item_id == 3335 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_MOLIBOX':
            status = 'modern_apparel_chest_PORT_PARTIAL' if item_id == 2224 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_CJDQBox':
            status = 'gift_of_the_hunk_PORT_PARTIAL' if item_id == 2905 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_JRDQBox':
            status = 'gift_of_the_beauty_PORT_PARTIAL' if item_id == 2904 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_XEZJ':
            status = 'malicious_horn_PORT_APPROX' if item_id == 3896 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'BlackDragon_Altar':
            status = 'black_dragon_altar_PORT_APPROX' if item_id == 266 and typ == '65' else 'lua_callback_not_implemented'
        elif cb == 'BlackDragon_Gem':
            status = 'black_dragon_altar_PORT_APPROX' if item_id in (864, 865, 866) and typ == '49' else 'lua_callback_not_implemented'
        elif cb == 'BlackDragon_Power':
            status = 'black_dragon_altar_PORT_APPROX' if item_id in (267, 268, 269, 270) and typ == '66' else 'lua_callback_not_implemented'
        elif cb == 'BlackDragon_Dice':
            status = 'black_dragon_altar_PORT_APPROX' if item_id == 271 and typ == '66' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_LoveBox':
            status = 'chest_of_fate_PORT_APPROX' if item_id == 2916 and typ == '31' else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_HonorPoint':
            status = 'honor_points_supported' if item_id == 2816 else 'lua_callback_not_implemented'
        elif cb == 'ItemUse_FightingPoint':
            status = 'chaos_points_supported' if item_id == 2951 else 'lua_callback_not_implemented'
        elif cb == 'AmplifierItem':
            status = ('amplifier_fairy_growth_pending' if item_id == 578 else
                      'amplifier_PORT_APPROX' if item_id in {48,56,898,993,998,1006,1128,3039,3094,3095,3096,3097,3844,3845,3880,3881,3882} else
                      'lua_callback_not_implemented')
        elif cb == 'NewbieChest':
            status = 'newbie_chest_PORT_APPROX' if 436 <= item_id <= 443 else 'lua_callback_not_implemented'
        elif cb == 'GiveRandomItems' or (cb == 'MysteryBox' and item_id in random_reward_items):
            status = 'random_reward_table_PORT_APPROX' if item_id in random_reward_items else 'lua_callback_not_implemented'
        elif cb == 'BlackMarket':
            status = 'black_market_table_PORT_APPROX' if item_id in black_market_items else 'lua_callback_not_implemented'
        elif cb in ('Corsairs_EquipmentChest', 'Corsairs_WeaponChest', 'Corsairs_WeaponChest2'):
            status = 'corsairs_chest_PORT_APPROX' if corsairs_chests.get(item_id) == cb else 'lua_callback_not_implemented'
        elif cb == 'ItemStat':
            status = 'item_stat_rule_PORT_APPROX' if item_id in stat_items else 'lua_callback_not_implemented'
        elif cb == 'GemVouchers' and 9612 <= item_id <= 9617:
            status = 'gem_voucher_PORT_APPROX'
        elif cb and cb != '0':
            status = 'lua_callback_not_implemented'
        elif equip:
            status = 'equipable_data'
        else:
            status = 'non_click_item_or_other_system'
        name = cols[1].strip().replace('\x00', '')
        records.append((item_id, name, typ, cb or '0', status, cols[28].strip()))
        by_status[status] += 1
        by_callback[cb or '0'] += 1
        by_type[typ] += 1
        if len(unresolved[cb]) < 3 and status in ('lua_callback_not_implemented','heal_no_formula','heal_state_PORT_PARTIAL','ticket_no_destination','skill_book_no_mapping','random_reward_table_PORT_APPROX','item_stat_rule_PORT_APPROX'):
            unresolved[cb].append(f'{item_id}: {name}')
    records.sort(key=lambda r: r[0])
    with Path(csv_output).open('w', encoding='utf-8', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['id','name_latin1','type','iteminfo_callback','offline_status','equip_slots'])
        writer.writerows(records)
    body = ['# Auditoria historica de itens — 05-AW','',
      f'Base: `{table.as_posix()}`; {len(records)} registros validos. A coluna de status e conservadora, nao executa Lua.',
      '', '## Status por categoria', '', '| Status | Registros |', '|---|---:|']
    for label,count in by_status.most_common(): body.append(f'| `{label}` | {count} |')
    body += ['', '## Tipos mais comuns', '', '| Tipo de iteminfo | Registros |', '|---|---:|']
    for label,count in by_type.most_common(15): body.append(f'| `{label}` | {count} |')
    body += ['', '## Callbacks mais comuns', '', '| Funcao iteminfo coluna 86 | Registros | Exemplos pendentes |', '|---|---:|---|']
    for cb,count in by_callback.most_common(32):
        body.append(f'| `{cb}` | {count} | {", ".join(unresolved.get(cb,[])).replace("|","/")} |')
    body += ['', '## Cuidados', '',
      '- `equipable_data` indica apenas metadados de slot; nao significa que todos os atributos foram validados.',
      '- `non_click_item_or_other_system` inclui itens de missao, materiais, crafting e outros sistemas. Nao e uma falha por si so.',
      '- `lua_callback_not_implemented` significa somente que nao ha equivalente deste callback em clique de inventario; verificar uso por outros servicos separadamente.',
      '- `skill_book_mapped` confirma apenas o mapeamento historico. Aprendizado exige classe, nivel, prerequisitos e TP.',
      '- `skill_book_name_match_PORT_APPROX` cobre livros sem `Server.Player.Skill` quando o nome do item coincide com uma única skill em `skillinfo.txt`; elegibilidade continua usando a regra carregada, mas o vínculo item-skill requer confirmação no servidor histórico.',
      '- `life_skill_book_mapped` confirma que o livro possui regra `Server.Life.Skill`; o fluxo offline verifica nível-alvo, pré-requisito, ponto profissional e material de recompensa.',
      '- `ticket_destination_PORT_APPROX` tem regra e destino histórico; o offline escolhe deterministicamente um ponto de nascimento quando o servidor escolhia um dos pontos aleatoriamente. `ticket_no_destination` conserva itens sem destino histórico (por exemplo, Old Ticket 3141).',
      '- `fairy_ration_supported` confirma uso manual via `Server.Fairy.Ration`; o cliente recupera `amount * 50` de stamina da fada equipada e persiste sua instância. `fairy_ration_auto_PORT_APPROX` cobre 2312/6842 no terceiro espaço da bolsa quando a fada equipada chega a `Server.Fairy.AutoFeed.Stamina`; o cliente verifica a cada atualização e não reproduz ainda o ciclo histórico de level/EXP da fada.',
      '- `fairy_egg_PORT_APPROX` cruza `Server.Fairy.Egg` com os itens tipo 59. O ovo exige dois slots livres, gera a fada histórica e a qualidade 4; a inicialização aleatória de atributos/stamina do `MakeItem` do servidor ainda está aproximada.',
      '- `fairy_mystery_egg_PORT_APPROX` cobre 1015: escolhe igualmente entre as dez fadas históricas 183..191/199 e grava três parâmetros de skill de fada históricos em cada instância de qualidade 4. A inicialização aleatória de atributos/stamina de `MakeItem` permanece aproximada.',
      '- `goddess_pouch_PORT_APPROX` cobre 1016: reproduz as oito recompensas fixas com probabilidade 1/12 cada e a distribuição uniforme 2530..2548 nos quatro resultados restantes, exigindo espaço e salvando a recompensa de qualidade 4. `MakeItem` e o anúncio global do servidor não têm paridade offline.',
      '- `aladdin_parcel_PORT_PARTIAL` cobre 2371: exige um espaço e reproduz as probabilidades marginais exatas do sorteio histórico de 20.000 resultados, com recompensas de qualidade 4. Os cinco IDs 19525/19538/19539/19540/19546 estão ausentes do catálogo local; quando um deles sai, o pacote é preservado. Anúncios globais e a sequência RNG não são reproduzidos.',
      '- `innocence_heart_D1` cobre 1009/1013: soma por 30 s 30% arredondado para baixo dos cinco atributos atuais e atualiza os atributos derivados. O item 1009 também entrega o Withered Heart 1010 em transação; estado temporário não persiste ao reiniciar.',
      '- `monster_summon_D1` cobre o Pergaminho de Invocação de Monstros 319: sorteia um grupo histórico de IDs, cria a criatura na posição do jogador e remove a invocação após 40 s. O modelo visual e parte dos atributos usam o catálogo local; exige validação em runtime.',
      '- `white_sheep_guardian_summon_PORT_APPROX` cobre 2990: rejeita barco e zona segura e invoca a criatura histórica ID 1009 usando Refresh 3700 e vida de 3600 s. A vida e os atributos usam os dados locais do monstro; os detalhes temporais de `CreateChaX`/Refresh permanecem aproximados.',
      '- `admiral_cloak_chest_PORT_APPROX` cobre o ID 9603: preserva a exigência histórica de dois espaços, recusa abertura no barco, consome o baú e entrega o ID 9604 com valor de qualidade 88. O bloqueio do servidor por inventário travado/estados de batalha não possui equivalente neste cliente offline.',
      '- `death_burden_D1` cobre 2919..2926: exige dois espaços e terra; sorteia 25% para a peça de classe e 75% para a peça comum, em qualidade 16. O callback H sempre concede 2930. A sequência Lua de `math.random` não é compartilhada pelo cliente offline.',
      '- `fairy_skill_book_supported` confirma livro mapeado em `Server.Fairy.Skill`; a fada guarda três pares skill/nível no save. O lote não altera a execução nem os efeitos dessas skills.',
      '- `fairy_fruit_PORT_PARTIAL` cobre os 15 IDs de frutas de fada. O cliente porta o ciclo histórico de crescimento, stamina, Fairy Coins, atributos da fada e limite de nível; RNG local e recálculo dos efeitos de equipamento da fada ainda aguardam runtime/paridade.',
      '- `blueprint_generator_PORT_APPROX` cobre 1000..1003 e o Constellation Blueprint 8909; este último usa o nível de energia padrão 1 do callback histórico. A seleção/rolagem offline mantém distribuição aproximada, e fluxo/save precisam de confirmação em runtime.',
      '- `fairy_skill_removal_D1` identifica os três livros 9609..9611; o cliente limpa apenas o par skill/nível selecionado na fada equipada e salva. Build confirmado; fluxo em runtime e save/reload ainda não foram exercitados.',
      '- `historical_noop_item_callback_D1` cobre `ItemUse_BZ`: o Lua somente bloqueia uso no barco; em terra não altera estado. Efeitos visuais eventuais não são definidos nesse callback e seguem pendentes de validação independente.',
      '- `fortune_lot_PORT_APPROX` cobre Fortune Lot (3087), com faixas normais e Mystic Clover, alteração de ouro/HP, pergaminho e bônus temporário de drop. A sequência RNG offline difere da Lua; o bônus de drop atual é global no cliente OpenGL, enquanto a mensagem histórica o descreve como regional, e estado temporário não sobrevive ao recarregamento.',
      '- `career_lot_PORT_APPROX` cobre Career Lot (3086), requisitos de nível/EXP, faixas normais e Mystic Clover, alteração imediata de EXP com level-up/recompensas, e estados aleatórios históricos. A sequência RNG offline difere da Lua; os bônus temporários não persistem após recarregar.',
      '- `fixed_summon_scroll_PORT_APPROX` cobre também Taurus Guardian Scroll (3036): cria a criatura histórica 1038 com os parâmetros literais de Refresh/Life, recusa barco e zona segura e só consome após a criação local. O significado temporal exato de `CreateChaX`/Refresh e a comparação visual continuam pendentes.',
      '- `holiday_pet_summon_PORT_APPROX` cobre 2888..2891: IDs históricos 930/931/932/929, limites de cinco pets, bloqueio no barco e durações de 30 min/1 h/2 h/3 h; as criaturas seguem o jogador. O efeito 361 e a semântica completa de host/target do servidor ainda não têm equivalente visual/offline.',
      '- `birth_cake_PORT_APPROX` cobre os IDs 3240/3338: `ItemUse_BirthCake` acrescenta 3 pontos de atributo fora do barco e persiste o total. O teto offline seguro de AP ainda precisa ser comparado ao servidor.',
      '- `gold_note_D1` cobre o ID 854: `ItemUse_CZHe` recusa a partir de 999.900.000 de ouro e concede exatamente 1.000.000 abaixo desse limite, conforme `CCharacter::AddMoney`.',
      '- `admiral_cloak_upgrade_D1` cobre o dispositivo 9605: exige terra e item tipo 88 abaixo do nível 5; `Creat_Item` inicia a capa em nível 1 com +1 nos cinco atributos, e `Cloak_Upgrade` aplica as chances históricas 100/90/80/70% e +1 em STR/AGI/DEX/CON/STA. Falha consome o dispositivo e não danifica a capa. Nível e atributos são salvos como instância.',
      '- `happy_book_supported` cobre os IDs 853/2989 com as faixas de EXP de `ItemUse_HappyBook`, usando DEXP histórico, bloqueio no barco, limite de nível e recompensas locais de promoção de nível.',
      '- `fixed_boss_set_chest_supported` cobre os IDs 2836..2841: valida quatro espaços livres, bloqueia uso no mar e entrega os três itens históricos de qualidade 4 em transação local.',
      '- `fixed_historical_chest_D1` cobre os baús históricos 192 e 209..221 transcritos em `ItemChestRules.cpp`; compatibilidade de tipo, recompensas, qualidade e espaço seguem os callbacks Lua. A implementação está registrada, mas os fluxos ainda precisam de confirmação em runtime/save.',
      '- `class_boss_weapon_chest_PORT_APPROX` cobre o ID 2842: nível 40, classes de segunda promoção e listas de armas históricas estão conectadas; o RNG offline ainda não replica a sequência Lua do servidor.',
      '- `black_dragon_altar_PORT_APPROX` cobre o altar 266, os três sacrifícios 864..866, poderes 267..270 e o dado 271. A seleção é feita clicando primeiro o material e depois o altar; os atributos, energia, recompensa e qualidade são persistidos. A ativação global do evento depende do servidor e fica habilitada localmente para teste; o RNG offline não compartilha a sequência Lua.',
      '- `fairy_march_PORT_APPROX` cobre o item 3335: exige personagens históricos tipo 3/4, recusa uso no barco e aplica DEF +50/SRec +20 por 600 s. A sequência RNG não se aplica; a representação visual do estado histórico ainda não foi portada.',
      '- `modern_apparel_chest_PORT_PARTIAL` cobre o callback 2224 e preserva o baú em falha; os 39 IDs de recompensa 5107..5153 usados nos doze conjuntos não existem em `iteminfo.txt`, então nenhum conjunto pode ser concedido sem os dados históricos ausentes.',
      '- `gift_of_the_hunk_PORT_PARTIAL` cobre o baú 2905: exige dez espaços, restringe personagens tipo 1/2 e transcreve as faixas de recompensa históricas; preserva o baú quando saem os IDs ausentes 936/5221..5223/5238..5240. RNG local não compartilha a sequência Lua.',
      '- `gift_of_the_beauty_PORT_PARTIAL` cobre o baú 2904: exige dez espaços, restringe personagens tipo 3/4 e transcreve as faixas de recompensa históricas; preserva o baú quando saem os IDs ausentes 937/5244..5247/5252..5255. RNG local não compartilha a sequência Lua.',
      '- `malicious_horn_PORT_APPROX` cobre o ID 3896: bloqueia o uso no barco e aplica ataque +30 por 1800 s. O estado visual histórico não foi portado.',
      '- `stat_reset_supported` cobre o ID 997: exige atributos base de pelo menos 5, bloqueia uso no barco, redefine os cinco atributos para 5, devolve a soma excedente a AP e salva. O limite seguro de AP offline ainda requer comparação com o servidor.',
      '- `foolish_exp_supported` cobre o ID 2953: soma 3% do intervalo DEXP do nível atual, bloqueia o uso no barco e aplica recompensas locais se o ganho atravessar o próximo nível; o arredondamento de `SetChaAttrI` ainda requer comparação em runtime.',
      '- `growth_fast_food_exp_supported` cobre o ID 992: exige nível 86, bloqueia o uso no barco e soma 1% do intervalo DEXP do nível atual; arredondamento e recarga ainda aguardam verificação em runtime.',
      '- `lucky_card_chest_PORT_APPROX` cobre o ID 3011: transcreve as oito chances raras em 241.920 e o sorteio comum entre cartas 959..984; sequência RNG e save/reload aguardam verificação em runtime.',
      '- `enchanting_card_chest_PORT_APPROX` cobre o ID 1035: exige um espaço livre, reproduz as três faixas raras e o sorteio de cartas 3850..3875 com fallback para 3850, qualidade 4 e save. O RNG local não compartilha a sequência Lua do servidor.',
      '- `enchanting_goddess_card_PORT_APPROX` cobre o ID 1036: exige um espaço e usa as cinco faixas literais de `ItemUse_mohuan` (pesos 8/9/10/200/773, incluindo 5 itens no resultado comum), qualidade 4 e entrega transacional. O RNG local não compartilha a sequência Lua e o anúncio global foi substituído por mensagem local.',
      '- `lantern_defense_D1` cobre 3107/3342: `State_DengLong_Add` acrescenta 50 DEF por 900/600 s; bloqueia uso navegando e segue a regra AddState de não renovar estado igual ativo. O temporizador é local e não sobrevive ao reinício do cliente.',
      '- `lost_christmas_cap_D1` cobre o ID 3888: aplica `STATE_PKSFYS` nível 1 (+30 DEF) por 300 s, bloqueia o uso navegando e respeita o estado de defesa mais forte já ativo. O temporizador é local e não persiste após reiniciar.',
      '- `grandiose_salute_PORT_APPROX` cobre o ID 1011: em terra, no horário local de 1 de janeiro entre 22h e 23h, reutiliza a tabela New Year Chest, exige nível 40 e uma vaga livre; fora do período preserva o item. O sorteio local não compartilha a sequência Lua.',
      '- `lucky_packet_PORT_APPROX` cobre 3901: concede 8/8.888 ouro, 100..300/66.600 EXP (divididos por 50 no nível 80+), sempre o Red Paper 3904 e uma chance 1/500 de carta ativa da `BaoXiang_HYBOX`. Distribuição e faixas são históricas; RNG e transição de nível CEXP ficam adaptados ao save offline e o limite local de ouro é 1.000.000.000.',
      '- `chest_of_fate_PORT_APPROX` cobre 2916: usa tipo 1/2 como masculino e 3/4 como feminino, preserva os sorteios do Lua, entrega itens de qualidade 4 e aplica VAL_STR aleatório (1..1.000) às peças especiais 2902/2903. RNG e sequência de MakeItem local não são idênticos ao servidor.',
      '- `amplifier_PORT_APPROX` cobre amplificadores de EXP/drop com multiplicadores, durações e limites de nível transcritos das descrições históricas; stacking e Lucky Strike ainda precisam da rotina de servidor, e 578 (crescimento de fada) permanece pendente.',
      '- `newbie_chest_PORT_APPROX` cobre os IDs 436..443 com condições e recompensas transacionais. Os modos de criação de instância `GiveItem` 4/95 e `MakeItem` 11 têm aproximações explícitas; consulte `MIGRATION_05-AZ.md` e `MIGRATION_05-BA.md` para os limites.',
      '- `ultimate_chest_D1` cobre os IDs 985..988 com níveis 10/35/45/55, elegibilidade por job/tipo, recompensas históricas de qualidade 4, quatro espaços livres e entrega transacional. Build confirmado; uso no jogo e save/reload ainda não foram exercitados.',
      '- `ascaron_chest_D1` cobre 1017: exige um espaço, consome o baú e entrega Gem of Soul 1012 qualidade 4; anúncio global não é reproduzido offline.',
      '- `christmas_gift_D1` cobre 2897: exige seis espaços e entrega os seis lotes históricos (incluindo 10 Exquisite Christmas cases e 99 Little Snow Balls) qualidade 4 em transação local.',
      '- `boat_speed_sail_D1` cobre 3046/3340: requer navegação, consome o item e aplica +15% à velocidade naval por 900 segundos, conforme `ItemUse_SanJiaoFan`/`State_YSBoatMspd_Add`. O estado é local e reinicia ao fechar o jogo; vinculação ao UID de barco e save/reload ainda não foram portados.',
      '- `forge_catalyst_D1` cobre 3074/3883: exige terra e aplica por 60 s o nível histórico 4 de `STATE_JLGLJB`, somando 0,4 à chance de `Forging_Success_Rate`; preview e confirmação de forja usam a mesma chance limitada a 100%.',
      '- `combine_catalyst_D1` cobre 3075/3884: exige terra e aplica por 60 s o nível histórico 2 de `STATE_HCGLJB`, somando 0,2 à chance de `Combine_Success_Rate` para gemas e refiners; preview e confirmação usam a mesma chance limitada a 100%. Buffs não persistem ao reiniciar; RNG local não compartilha a sequência do servidor.',
      '- `party_exp_fruit_PORT_APPROX` cobre 849/6019: aplica EXP x1,5 por 900 s ao personagem offline solo. O estado é runtime-only; o serviço histórico de grupo, que propaga o buff a todos os membros e barcos, não existe no cliente offline.',
      '- `reincarnation_pagoda_D1` cobre 240: cria a Fairy of Life 183 qualidade 15, fixa cinco atributos em 5 conforme `SI/SN/SE` e `SA1..SA5`, valida espaço, consome e salva. O modelo offline mapeia os cinco atributos para STR/CON/AGI/DEX/STA; ainda falta confirmar o mapeamento de todos os slots de atributo do servidor em runtime.',
      '- `fixed_gem_voucher_D1` cobre 856..859, 911, 914..916 e 918..921: cada voucher exige um espaço e entrega a gema ID/qualidade 4 literal em `ItemEffect.lua`; a bolsa e o save usam a transação local já existente.',
      '- `black_dragon_grand_prize_D1`/`black_dragon_first_prize_D1` cobrem 272/273 com as três peças Black Dragon qualidade 16 e o Happy Holiday Magazine qualidade 4, respectivamente. `black_dragon_second_prize_PORT_PARTIAL` cobre 274: exige 14 espaços, entrega 10 Strengthening Crystals e reproduz os oito conjuntos históricos; quatro resultados que incluem IDs ausentes 5034..5038 preservam o baú sem consumir nada. Os quatro resultados completos entregam as peças disponíveis com qualidade 4. `black_dragon_third_prize_D1` cobre 275 com sorteio histórico 50/50 entre dois amplificadores e o Lucky Gift Parcel. Avisos globais e RNG do servidor não são reproduzidos.',
      '- `random_reward_table_PORT_APPROX` indica peso/quantidade/qualidade carregados da tabela historica. A sequência do RNG, atributos de instância de equipamento e anúncio global ainda não têm paridade comprovada; veja `MIGRATION_05-BC_RANDOM_REWARD_ITEMS.md`.',
      '- `black_market_table_PORT_APPROX` confirma que o cliente leu os pesos/quantidades/qualidade da tabela histórica de Black Market. O gerador aleatório e os avisos são locais e não reproduzem a sequência/Notice do servidor.',
      '- `corsairs_chest_PORT_APPROX` cruza os IDs e recompensas fixas/aleatórias dos callbacks históricos de baús. Caixas de equipamento randomizam uma escolha offline; a rolagem não compartilha a sequência de `math.random` do servidor, e a geração de atributos de instância `chInstance` permanece pendente.',
      '- `item_stat_rule_PORT_APPROX` aplica as dez regras históricas de ajuste de atributo em saves offline. A restrição original de stall não tem equivalente no cliente offline atual; veja `MIGRATION_05-BD_ITEM_STAT.md`.',
      '- `gem_voucher_PORT_APPROX` usa as seis listas históricas de `GemVouchers`; o RNG mantém a distribuição uniforme, mas não a sequência Lua, e `Notice` é local no cliente offline; veja `MIGRATION_05-BG_GEM_VOUCHERS.md`.',
      '- `heal_amount_supported` e `heal_percentage_supported` aplicam valor absoluto ou a parcela percentual e respeitam o cooldown local. `heal_state_supported` cobre os estados historicos 165..167 de comidas e poções; estados com outros IDs permanecem `heal_state_PORT_PARTIAL`.',
      '- `kala_stamina_state_D1` cobre Kara’s Victory (1021): aplica por 20 s o valor STA atual como bônus temporário, sem alterar os executores de efeitos de skills. O estado é local à sessão e não sobrevive ao reinício.',
      '- `boat_food_SP_PORT_APPROX` cobre Fish Meat (4551): exige navegação e recupera até 50 SP, limitado ao máximo. Os campos offline de SP do personagem substituem os atributos da embarcação; runtime e persistência da embarcação ainda precisam de validação.',
      '- Texto original de iteminfo usa encoding legado; CSV conserva bytes decodificados como latin1 para nao descartar registros.',
      '- Ferramenta nao modifica saves, nao converte recursos e nao executa scripts Lua.', '']
    Path(md_output).write_text('\n'.join(body),encoding='utf-8')
    return len(records), by_status

if __name__=='__main__':
    if len(sys.argv)!=4:
        raise SystemExit('Usage: item_action_audit.py Client/scripts report.csv report.md')
    n,c=audit(*sys.argv[1:]); print('Audited',n,'items:',dict(c))
