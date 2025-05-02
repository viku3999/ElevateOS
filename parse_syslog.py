import re
from datetime import datetime
from collections import defaultdict

# Adjust this to match your actual syslog file path
LOG_FILE = "syslog.txt"

# Regex to parse log lines with microsecond timestamps
# Example format: May 02 00:30:45.123456 ...
LAUNCH_RE = re.compile(r"(May \d+ \d+:\d+:\d+\.\d+).*task launch thread id: (\d+)")
COMPLETE_RE = re.compile(r"(May \d+ \d+:\d+:\d+\.\d+).*task completed thread id: (\d+)")

# Track launch times by thread
launch_times = defaultdict(list)
exec_times = defaultdict(list)

# Parse log lines
with open(LOG_FILE, "r") as f:
    for line in f:
        launch_match = LAUNCH_RE.search(line)
        complete_match = COMPLETE_RE.search(line)

        if launch_match:
            timestamp_str, tid = launch_match.groups()
            timestamp = datetime.strptime(timestamp_str, "%b %d %H:%M:%S.%f")
            launch_times[tid].append(timestamp)

        elif complete_match:
            timestamp_str, tid = complete_match.groups()
            timestamp = datetime.strptime(timestamp_str, "%b %d %H:%M:%S.%f")
            if launch_times[tid]:
                launch_time = launch_times[tid].pop(0)
                exec_duration_us = (timestamp - launch_time).total_seconds() * 1_000_000  # in microseconds
                exec_times[tid].append(exec_duration_us)

# Print results
print("Service Execution Statistics (by Thread ID):")
for tid, times in exec_times.items():
    if times:
        min_time = min(times)
        max_time = max(times)
        avg_time = sum(times) / len(times)
        jitter = max_time - min_time
        print(f"\nThread ID: {tid}")
        print(f"  Min Execution Time   : {min_time:.3f} µs")
        print(f"  Max Execution Time   : {max_time:.3f} µs")
        print(f"  Avg Execution Time   : {avg_time:.3f} µs")
        print(f"  Jitter               : {jitter:.3f} µs")
        print(f"  Execution Count      : {len(times)}")
