
import sys
import re
from collections import Counter

def parse_traces(filename):
    # key: (instr_a, instr_b), value: Counter of (disasm_a, disasm_b)
    pairs = {}
    
    with open(filename, 'r') as f:
        lines = f.readlines()
    
    current_stack = [] # (depth, opcode, ip, disasm)
    
    for line in lines:
        if '[LLC_MISS_TRACE' in line:
            current_stack = []
            continue
        
        if '--------------------------------------------------------' in line:
            current_stack = []
            continue
        
        # Match something like "  [RDX] <- 0x7fd4edaec0d1 : add rdx, 0x10"
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
                # Child depends on Parent (current line is Parent)
                # Program order: Parent then Child
                parent_instr = instr
                parent_ip = ip
                parent_disasm = disasm
                
                # We want pairs that are close in program order
                dist = child_ip - parent_ip
                if 0 < dist < 32: # Reasonably close and in order
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
