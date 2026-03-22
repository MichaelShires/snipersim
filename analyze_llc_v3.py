
import sys
import re
from collections import Counter

def parse_traces(filename):
    pairs = {}
    with open(filename, 'r') as f:
        lines = f.readlines()
    
    current_stack = [] 
    for line in lines:
        if '[LLC_MISS_TRACE' in line:
            current_stack = []
            continue
        if '--------------------------------------------------------' in line:
            current_stack = []
            continue
        
        match = re.search(r'^(?P<indent>\s*)(?:\[.*\] <- )?0x(?P<ip>[0-9a-f]+) : (?P<instr>\w+)(?P<rest>.*)', line)
        if match:
            indent = len(match.group('indent'))
            depth = indent // 2
            ip = int(match.group('ip'), 16)
            instr = match.group('instr')
            disasm = instr + match.group('rest')
            
            while current_stack and current_stack[-1][0] >= depth:
                current_stack.pop()
            
            if current_stack:
                child_depth, child_instr, child_ip, child_disasm = current_stack[-1]
                parent_instr = instr
                parent_ip = ip
                parent_disasm = disasm
                
                # We want pairs that are close in memory, regardless of which one is "parent"
                # Wait, parent is the one that produces the value.
                # If they are consecutive in program order, Parent should be before Child.
                dist = child_ip - parent_ip
                if abs(dist) < 32: 
                    key = (parent_instr, child_instr)
                    if key not in pairs:
                        pairs[key] = Counter()
                    pairs[key][(parent_disasm, child_disasm, dist)] += 1
            
            current_stack.append((depth, instr, ip, disasm))
            
    return pairs

if __name__ == "__main__":
    filename = 'sniper_llc_output.txt'
    pairs = parse_traces(filename)
    
    flattened = []
    for (p_ins, c_ins), counter in pairs.items():
        for (p_dis, c_dis, dist), count in counter.items():
            flattened.append(((p_ins, c_ins, p_dis, c_dis, dist), count))
    
    flattened.sort(key=lambda x: x[1], reverse=True)
    
    print(f"{'Count':6} | {'Distance':8} | {'Parent Instruction':30} -> {'Child Instruction'}")
    print("-" * 100)
    for ((p_ins, c_ins, p_dis, c_dis, dist), count) in flattened[:30]:
        print(f"{count:6} | {dist:8} | {p_dis:30} -> {c_dis}")
