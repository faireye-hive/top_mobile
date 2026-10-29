"""Synthetic Win32 scene table fixtures; no Client or archived sources needed."""
import csv
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest

from cryptography.exceptions import InvalidTag
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from Tools import scene_table_audit as table


# Member order follows TableData.h:39-51 and EffectSet.h:108-149. These are
# format fixtures, not a runtime lookup table; the audit parses supplied sources.
RAW = """class CRawDataInfo {
BOOL bExist{ false };
int nIndex{ 0 };
char szDataName[72]{ "" };
DWORD dwLastUseTick;
BOOL bEnable;
void* pData;
DWORD dwPackOffset;
DWORD dwDataSize;
int nID;
DWORD dwLoadCnt;
};
void read() { ParamList.push_back(pstrList[i + 2]); }
"""
EFFECT = """class CMagicInfo : public CRawDataInfo {
CMagicInfo() { nPhotoTexID = 0; memset(nDummy,-1,sizeof(nDummy)); }
char szName[16];
char szPhotoName[16];
int nPhotoTexID;
int nEffType;
int nObjType;
int nDummyNum;
int nDummy[8];
int nDummy2;
int nHeightOff;
float fPlayTime;
int LightID;
float fBaseSize;
};
class CMagicSet {
void read() {
strncpy(pInfo->szName, ParamList[0].c_str(), sizeof(pInfo->szName));
strncpy(pInfo->szPhotoName, ParamList[1].c_str(), sizeof(pInfo->szPhotoName));
pInfo->nEffType = Str2Int(ParamList[2].c_str());
pInfo->nObjType = Str2Int(ParamList[3].c_str());
pInfo->nDummyNum = Util_ResolveTextLine(ParamList[4].c_str(), lstr1, 8, ',');
pInfo->nDummy[n] = Str2Int(lstr1[n].c_str());
if (pInfo->nDummyNum == 1) {
if (pInfo->nDummy[n] < -1) { pInfo->nDummyNum = 0; }
}
pInfo->nDummy2 = Str2Int(ParamList[5].c_str());
pInfo->nHeightOff = Str2Int(ParamList[6].c_str());
pInfo->fPlayTime = Str2Float(ParamList[7].c_str());
pInfo->LightID = Str2Int(ParamList[8].c_str());
pInfo->fBaseSize = Str2Float(ParamList[9].c_str());
}
};
"""
# Synthetic public test key/IV, unrelated to Client constants.
CPP = """const unsigned char cluTableKey[] = {
0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f};
const unsigned char cluTableIV[] = {
0x0f,0x0e,0x0d,0x0c,0x0b,0x0a,0x09,0x08,0x07,0x06,0x05,0x04,0x03,0x02,0x01,0x00};
CryptoPP::GCM<CryptoPP::AES>::Decryption d;
d.SetKeyWithIV(cluTableKey, 16, cluTableIV, 16);
CryptoPP::AuthenticatedDecryptionFilter df(d, sink, CryptoPP::AuthenticatedDecryptionFilter::DEFAULT_FLAGS, 12);
"""
UTIL = """int Str2Int(const char* s) { return atoi(s); }
float Str2Float(const char* s) { return float(atof(s)); }
"""


class SceneTableAuditTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.client, self.original, self.out = [self.root / name for name in ("Client", "original", "out")]
        for relative, source in {
            "Libraries/common/include/TableData.h": RAW,
            "Libraries/common/src/TableData.cpp": CPP,
            "Client/src/EffectSet.h": EFFECT,
            "Libraries/Util/include/util2.h": UTIL,
        }.items():
            path = self.original / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(source)
        self.tables = self.client / "scripts/table"
        self.tables.mkdir(parents=True)
        (self.client / "effect").mkdir()
        (self.client / "effect/Foo.par").write_bytes(b"fixture")
        self.txt = self.tables / "sceneffectinfo.txt"
        self.txt.write_text("1\tFoo.par\tName\tPhoto\t2\t3\t1,4\t-1\t5\t0.1\t6\t1.25\n")
        self.fields, self.size = table.source_layout(RAW, EFFECT)
        self.crypto = table.source_crypto(CPP)
        records = table.txt_rows(self.txt, self.fields, table.semantic_columns(EFFECT))
        self.plain = bytearray(self.size * 3)
        for name, value in dict(records[1], bExist=1, nIndex=1, nID=1).items():
            self.put(name, value)
        self.bin = self.tables / "sceneffectinfo.bin"
        self.encrypt(self.plain)

    def put(self, name, value):
        offset, fmt, count = self.fields[name]
        offset += self.size
        if fmt == "s":
            self.plain[offset:offset + count] = value.encode().ljust(count, b"\0")[:count]
        else:
            struct.pack_into("<" + fmt * count, self.plain, offset,
                             *([value] if count == 1 else value))

    def encrypt(self, plain):
        encryptor = Cipher(algorithms.AES(self.crypto[0]), modes.GCM(self.crypto[1])).encryptor()
        ciphertext = encryptor.update(bytes(plain)) + encryptor.finalize()
        self.bin.write_bytes(ciphertext + encryptor.tag[:12])

    def run_audit(self, **kwargs):
        return table.audit(self.client, self.original, self.out, **kwargs)

    def test_source_layout_and_authenticated_semantic_roundtrip(self):
        self.assertEqual(self.size, 208)
        self.assertEqual(self.fields["pData"], (88, "I", 1))
        self.assertEqual(self.fields["szName"], (108, "s", 16))
        summary = self.run_audit()
        self.assertEqual((summary["slots"], summary["bin_records"], summary["different_fields"]), (3, 1, 0))
        self.assertTrue(summary["authenticated"])
        self.assertEqual(json.loads((self.out / "summary.json").read_text()), summary)
        with (self.out / "scene_differences.csv").open() as f:
            self.assertEqual(list(csv.DictReader(f)), [])

    def test_bad_tag_rejected_before_output(self):
        data = self.bin.read_bytes()
        self.bin.write_bytes(data[:-1] + bytes([data[-1] ^ 1]))
        with self.assertRaises(InvalidTag):
            self.run_audit()
        self.assertFalse(self.out.exists())

    def test_authenticated_partial_record_rejected(self):
        self.encrypt(self.plain[:-1])
        with self.assertRaisesRegex(ValueError, "Win32 layout"):
            self.run_audit()
        self.assertFalse(self.out.exists())

    def test_active_slot_mismatch_rejected(self):
        self.put("nID", 2)
        self.encrypt(self.plain)
        with self.assertRaisesRegex(ValueError, "index mismatch"):
            self.run_audit()
        self.assertFalse(self.out.exists())

    def test_source_contract_changes_are_not_guessed(self):
        with self.assertRaisesRegex(ValueError, "unsupported source member"):
            table.source_layout(RAW.replace("DWORD dwLoadCnt;", "double dwLoadCnt;"), EFFECT)
        with self.assertRaisesRegex(ValueError, "tag contract"):
            table.source_crypto(CPP.replace("DEFAULT_FLAGS, 12", "DEFAULT_FLAGS, 16"))
        with self.assertRaisesRegex(ValueError, "dummy list reader"):
            table.semantic_columns(EFFECT.replace("pInfo->nDummy[n] = Str2Int", "pInfo->nDummy[n] = atoi"))
        with self.assertRaisesRegex(ValueError, "dummy defaults/count"):
            table.check_dummy_contract(EFFECT.replace("< -1", "< 0"), self.fields)

    def test_changed_name_and_scalar_reported_without_alias(self):
        self.put("szDataName", "foo.par")
        self.put("nHeightOff", 19)
        self.encrypt(self.plain)
        summary = self.run_audit()
        self.assertEqual((summary["different_ids"], summary["different_fields"], summary["case_only_assets"]), (1, 2, 1))
        with (self.out / "scene_differences.csv").open() as f:
            self.assertEqual({r["field"] for r in csv.DictReader(f)}, {"szDataName", "nHeightOff"})

    def test_missing_channels_and_server_are_separate_evidence(self):
        self.put("szDataName", "absent.par")
        self.encrypt(self.plain)
        channels = self.root / "channels.csv"
        channels.write_text("skill_id,channel,effect_id,status\n7,target,1,missing_par\n8,actor,2,no_scene_par\n")
        server = self.root / "server.txt"
        server.write_text("2\tFoo.par\tName\tPhoto\t2\t3\t-2\t-1\t0\t1\t0\t1\n")
        self.assertEqual(self.run_audit(binding=channels, server_table=server)["unresolved_channels_examined"], 2)
        with (self.out / "unresolved_channels.csv").open() as f:
            rows = list(csv.DictReader(f))
        self.assertEqual(rows[0]["classification"], "mode_or_asset_resolution_differs")
        self.assertEqual(rows[1]["classification"], "absent_in_client_txt_and_bin")
        self.assertEqual(rows[1]["server_name"], "Foo.par")
        self.assertTrue(all(r["runtime_use_proven"] == "no" for r in rows))

    def test_atoi_atof_dummy_and_no_client_output(self):
        self.assertEqual(table.dx9_split("1,,4,", ","), ["1", "4"])
        self.assertEqual(table.dx9_split(",,4,", ","), ["", "4"])
        self.assertEqual(table.dx9_split(",", ","), [","])
        self.assertEqual(table.dx9_int(" -12.5junk"), -12)
        self.assertEqual(table.dx9_int("invalid"), 0)
        self.assertEqual(table.dx9_float("0.1suffix"), struct.unpack("<f", struct.pack("<f", 0.1))[0])
        self.txt.write_text(self.txt.read_text().replace("1,4", "-2"))
        row = table.txt_rows(self.txt, self.fields, table.semantic_columns(EFFECT))[1]
        self.assertEqual(row["nDummyNum"], 0)
        self.assertEqual(row["nDummy"], [-2] + [-1] * 7)
        with self.assertRaisesRegex(ValueError, "outside Client"):
            table.audit(self.client, self.original, self.client / "audit")
        self.assertFalse((self.client / "audit").exists())

    def test_missing_literal_in_both_modes_is_not_extension_repaired(self):
        self.txt.write_text(self.txt.read_text().replace("Foo.par", "Foo.pa"))
        self.put("szDataName", "Foo.pa")
        self.encrypt(self.plain)
        channels = self.root / "channels.csv"
        channels.write_text("skill_id,channel,effect_id,status\n7,target,1,missing_par\n")
        self.assertEqual(self.run_audit(binding=channels)["missing_literal_assets"], 1)
        with (self.out / "unresolved_channels.csv").open() as f:
            row = next(csv.DictReader(f))
        self.assertEqual(row["classification"], "missing_literal_asset_in_both_modes")
        self.assertEqual(row["bin_name"], "Foo.pa")

    def test_record_present_in_only_one_mode_is_reported(self):
        self.put("bExist", 0)
        self.encrypt(self.plain)
        summary = self.run_audit()
        self.assertEqual((summary["bin_records"], summary["txt_records"], summary["different_ids"]), (0, 1, 1))
        with (self.out / "scene_records.csv").open() as f:
            row = next(csv.DictReader(f))
        self.assertEqual(row["asset_lookup"], "no_scene_record")
        self.assertEqual(row["txt_asset_lookup"], "exact")


if __name__ == "__main__":
    unittest.main()
