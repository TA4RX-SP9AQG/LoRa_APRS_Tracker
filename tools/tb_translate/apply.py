"""Wraps on-screen text in src/*.cpp with TB_T("English", "Turkce") using tools/tb_translate/strings_tr.py.

Usage:  python tools/tb_translate/apply.py            (report only)
        python tools/tb_translate/apply.py --write    (rewrite the sources)

Only string literals whose text (minus the "> " marker and blanks around it) is in the dictionary are touched, and
never when the literal is compared, logged, printed to the serial port, sent over the radio, or used as a file
name. Already wrapped literals are skipped, so the script can be re-run safely.
"""
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from strings_tr import TR  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent.parent
WRITE = "--write" in sys.argv
FILES = ["menu_utils", "keyboard_utils", "display", "msg_utils", "LoRa_APRS_Tracker", "station_utils", "gps_utils",
         "power_utils", "battery_utils", "sleep_utils", "wifi_utils", "utils"]
Q = chr(34)
BS = chr(92)
LITERAL = re.compile(Q + "((?:[^" + Q + BS + "\n]|" + BS + ".)*)" + Q)

# immediately left of the literal: comparisons and lookups
LEFT_BLOCK = ("==", "!=", ".equals(", "startsWith(", "endsWith(", "indexOf(", "strcmp(", "compareTo(", "contains(",
              "find(", "TB_T(")
# anywhere earlier in the same statement
STATEMENT_BLOCK = ("logger.log(", "Serial.print", "sendNewPacket(", "addToOutputBuffer(", "saveNewMessage(", "SPIFFS.",
                   "sendMessage(", "ESP_LOG", "printf(")


def split(text):
    m = re.match("^([ ]*(?:>[ ]*)?)(.*?)([ ]*)$", text)
    return m.group(1), m.group(2), m.group(3)


def translate(text):
    prefix, core, suffix = split(text)
    if core in TR:
        new_core = TR[core]
        # right-aligned text (many leading blanks): keep its last column where it was
        if prefix.strip() == "" and len(prefix) >= 6:
            prefix = " " * max(1, len(prefix) + len(core) - len(new_core))
        return prefix + new_core + suffix
    # pieces of a padded line such as "<Back      (0" or ")     Enter>": only the key words, same length
    out = text
    for en, tr in (("Back", "Geri"), ("Enter", "Tamam")):
        if (text.startswith("<" + en) or text.endswith(en + ">")) and ("<" in text or ">" in text):
            out = out.replace(en, tr)
    if text.startswith("<Clear Body"):
        out = out.replace("<Clear Body", "<Govde Sil ")
    if text.endswith("Select>") and text.startswith("<Back"):
        out = out.replace("Select>", "Sec>   ")
    if out != text:
        return out
    return None


def blocked(src, start):
    left = src[max(0, start - 40) : start].rstrip()
    if left.endswith(LEFT_BLOCK):
        return "compared/looked up"
    stmt_start = max(src.rfind(c, 0, start) for c in ";{}")
    stmt = src[stmt_start + 1 : start]
    for token in STATEMENT_BLOCK:
        if token in stmt:
            return "logged/sent: " + token
    return None


def process(path):
    src = path.read_text(encoding="utf-8", errors="surrogateescape")
    edits = []
    skipped = []
    for m in LITERAL.finditer(src):
        text = m.group(1)
        if BS in text or "%" in text and False:
            continue
        new = translate(text)
        if new is None:
            continue
        reason = blocked(src, m.start())
        if reason:
            skipped.append((text, reason))
            continue
        if len(new.strip()) > len(text.strip()) + 3:
            print(f"  WARNING longer: {text!r} -> {new!r}")
        edits.append((m.start(), m.end(), "TB_T(" + Q + text + Q + ", " + Q + new + Q + ")"))
    if WRITE and edits:
        out = src
        for a, b, call in sorted(edits, reverse=True):
            out = out[:a] + call + out[b:]
        if "tb_lang.h" not in out:
            idx = out.find("#include ")    # the first include: later ones may sit inside an #ifdef
            eol = out.find("\n", idx)
            out = out[: eol + 1] + '#include "tb_lang.h"\n' + out[eol + 1 :]
        path.write_text(out, encoding="utf-8", errors="surrogateescape")
    return edits, skipped


def main():
    total = 0
    for name in FILES:
        p = ROOT / "src" / (name + ".cpp")
        if not p.exists():
            continue
        edits, skipped = process(p)
        total += len(edits)
        print(f"{name}: {len(edits)} wrapped, {len(skipped)} left alone")
        for text, reason in skipped:
            print(f"    skipped {text!r}: {reason}")
    print(("WROTE " if WRITE else "would wrap ") + str(total) + " literals")


if __name__ == "__main__":
    main()
