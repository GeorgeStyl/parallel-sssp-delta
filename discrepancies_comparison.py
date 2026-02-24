import sys
import re
import os

def analyze_sssp_results():
    serial_file = "serial.txt"
    parallel_file = "parallel.txt"
    log_files = ["thread_0.log", "thread_1.log", "thread_2.log", "thread_3.log"]

    print("=== SSSP AUTOMATED DEBUGGER ===")
    
    # 1. Check Output Accuracy
    if os.path.exists(serial_file) and os.path.exists(parallel_file):
        print("\n[STEP 1] Comparing SSSP Outputs...")
        with open(serial_file, 'r') as fs, open(parallel_file, 'r') as fp:
            s_data = {l.split(':')[0].strip(): l.split(':')[1].strip() for l in fs if ':' in l}
            p_data = {l.split(':')[0].strip(): l.split(':')[1].strip() for l in fp if ':' in l}
        
        nodes = sorted(set(s_data.keys()) | set(p_data.keys()), key=int)
        max_err = 0.0
        max_node = None
        
        for n in nodes:
            sv_str, pv_str = s_data.get(n, "INF"), p_data.get(n, "INF")
            sv = float('inf') if "INF" in sv_str.upper() else float(sv_str)
            pv = float('inf') if "INF" in pv_str.upper() else float(pv_str)
            
            if sv != pv and sv != 0 and sv != float('inf'):
                err = abs(pv - sv) / sv * 100
                if err > max_err:
                    max_err, max_node = err, n
        
        if max_err < 1e-7:
            print(">> SUCCESS: Perfect match (0.000% error)!")
        else:
            print(f">> FAILURE: Max Error {max_err:.6f}% at Node {max_node}")
    else:
        print("[STEP 1] SKIP: serial.txt or parallel.txt not found.")

    # 2. Check Thread Synchronization
    print("\n[STEP 2] Analyzing Thread Logs for Divergence...")
    all_logs = []
    for f in log_files:
        if os.path.exists(f):
            with open(f, 'r') as file:
                all_logs.append(file.readlines())
    
    if len(all_logs) == 4:
        min_lines = min(len(l) for l in all_logs)
        diverged = False
        for i in range(min_lines):
            actions = [l[i].split("]: ")[1].strip() for l in all_logs]
            # Ignore the specific size numbers in "size=X" for basic sync check
            tags = [re.sub(r'size=\d+', 'size=X', a) for a in actions]
            
            if len(set(tags)) > 1:
                print(f"!! DIVERGENCE DETECTED AT LOG LINE {i+1} !!")
                for tid, act in enumerate(actions):
                    print(f"   TID {tid}: {act}")
                diverged = True
                break
        if not diverged:
            print(">> SUCCESS: All threads remained in perfect lockstep.")
    else:
        print(f"[STEP 2] SKIP: Only {len(all_logs)}/4 log files found.")

if __name__ == "__main__":
    analyze_sssp_results()