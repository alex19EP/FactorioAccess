import re, sys, json, pathlib
src = pathlib.Path(sys.argv[1])
text = "".join(p.read_text(encoding="utf-8") for p in src.rglob("*.cpp"))
S = r'((?:"[^"]*"\s*)+)'
join = lambda s: "".join(re.findall(r'"([^"]*)"', s))
addr = [(f, join(s)) for f, s in re.findall(r'\baddress\(\s*(?:layout\.)?(\w+)\s*,\s*' + S + r'\)', text)]
slots = [("agui::Widget", join(m), f) for f, m in re.findall(r'\bslot\(\s*(?:layout\.)?(\w+)\s*,\s*' + S + r'\)', text)]
slots += [(join(t), join(m), f) for f, t, m in re.findall(r'\bclassSlot\(\s*(?:layout\.)?(\w+)\s*,\s*' + S + r',\s*' + S + r'\)', text)]
json.dump({"addresses": addr, "slots": slots}, sys.stdout, indent=0)
