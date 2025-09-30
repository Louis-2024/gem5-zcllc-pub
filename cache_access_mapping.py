from collections import defaultdict

in_path = "cache_next_access.log"
out_path = "cache_access_map.txt"

access_map = defaultdict(list)
with open(in_path, "r", encoding="utf-8") as f:
    for raw in f:
        line = raw.strip()
        if not line:
            continue
        addr_str, time_str = map(str.strip, line.split(",", 1))
        access_map[addr_str].append(int(time_str))

for times in access_map.values():
    times.sort()

ordered_items = sorted(access_map.items(), key=lambda kv: kv[1][0])

with open(out_path, "w", encoding="utf-8") as out:
    for addr, times in ordered_items:
        out.write(", ".join([addr] + [str(t) for t in times]) + "\n")
