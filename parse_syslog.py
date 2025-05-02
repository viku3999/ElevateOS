import re
from datetime import datetime
from collections import defaultdict

LOG_FILE = "syslog.txt"

# Expected deadlines in microseconds (example: 100ms = 100_000us)
DEADLINES_US = {
    "290976": 100_000,
    "290977": 100_000,
    "290978": 100_000,
}

LAUNCH_RE = re.compile(r"(\d{4}-\d{2}-\d{2} \d+:\d+:\d+\.\d{6}) .*task launch thread id: (\d+)")
COMPLETE_RE = re.compile(r"(\d{4}-\d{2}-\d{2} \d+:\d+:\d+\.\d{6}) .*task completed thread id: (\d+)")

launch_times = defaultdict(list)
deadline_misses = defaultdict(list)

with open(LOG_FILE, "r") as f:
    for line in f:
        match = LAUNCH_RE.search(line)
        if match:
            timestamp_str, tid = match.groups()
            timestamp = datetime.strptime(timestamp_str, "%Y-%m-%d %H:%M:%S.%f")

            if launch_times[tid]:
                # Time since last launch
                prev = launch_times[tid][-1]
                delta_us = (timestamp - prev).total_seconds() * 1_000_000  # microseconds
                expected = DEADLINES_US.get(tid, None)
                if expected and delta_us > expected:
                    deadline_misses[tid].append(delta_us)

            launch_times[tid].append(timestamp)

# Report deadline misses
for tid, misses in deadline_misses.items():
    print(f"\nThread ID: {tid}")
    print(f"  Deadline: {DEADLINES_US[tid]/1000:.3f} ms")
    print(f"  Misses  : {len(misses)}")
    for i, miss in enumerate(misses, 1):
        print(f"    Miss {i}: {miss/1000:.3f} ms between launches")
