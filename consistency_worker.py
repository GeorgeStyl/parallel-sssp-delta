import re

def parse_log(filename):
    """
    Parses the log file into a list of events.
    Each event is a tuple: (timestamp, bucket_info, action_text)
    """
    events = []
    # Regex to capture: [timestamp] [TID] [BucketInfo]: Action
    # Example: [14:17:51.705] [TID 0]: B0: Light phase iteration start
    pattern = re.compile(r"\[(.*?)\] \[TID \d+\]: (?:(B\d+): )?(.*)")
    
    try:
        with open(filename, 'r') as f:
            for line in f:
                match = pattern.search(line)
                if match:
                    timestamp, bucket, action = match.groups()
                    events.append({
                        'time': timestamp,
                        'bucket': bucket if bucket else "N/A",
                        'action': action.strip()
                    })
    except FileNotFoundError:
        print(f"Error: {filename} not found.")
        return []
    return events

def check_logs():
    log_files = ["thread_0.log", "thread_1.log", "thread_2.log", "thread_3.log"]
    all_thread_events = [parse_log(f) for f in log_files]

    # Find the maximum length to compare
    max_len = max(len(events) for events in all_thread_events)
    
    print(f"{'Event #':<8} | {'Status':<10} | {'Details'}")
    print("-" * 60)

    for i in range(max_len):
        current_step_data = []
        for t_idx, events in enumerate(all_thread_events):
            if i < len(events):
                current_step_data.append(events[i])
            else:
                current_step_data.append(None)

        # Check for existence (did a thread exit early?)
        if any(e is None for e in current_step_data):
            missing = [f"TID {j}" for j, e in enumerate(current_step_data) if e is None]
            print(f"{i:<8} | FAIL       | Threads {', '.join(missing)} exited early or missed events.")
            continue

        # Check for Logic Consistency (Bucket and Action)
        # We compare everyone to Thread 0
        ref = current_step_data[0]
        mismatch = False
        for j in range(1, 4):
            if current_step_data[j]['action'] != ref['action'] or \
               current_step_data[j]['bucket'] != ref['bucket']:
                mismatch = True
                break
        
        if mismatch:
            print(f"{i:<8} | MISMATCH   | Threads diverged at {ref['bucket']}. Action: {ref['action']}")
            for j, e in enumerate(current_step_data):
                print(f"         -> TID {j}: {e['bucket']} - {e['action']}")
        else:
            # Check for Timestamp delta (optional, but shows if one thread is lagging)
            # Usually timestamps won't be identical to the millisecond, but actions MUST be.
            pass

    print("-" * 60)
    print("Check complete. If 'MISMATCH' appeared, your barriers are being bypassed.")

if __name__ == "__main__":
    check_logs()