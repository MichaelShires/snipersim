
import sys
import re
from collections import Counter

def parse_traces(filename):
    pairs = Counter()
    
    with open(filename, 'r') as f:
        lines = f.readlines()
    
    current_stack = [] # (depth, opcode)
    
    for line in lines:
        if '[LLC_MISS_TRACE' in line:
            current_stack = []
            continue
        
        if '--------------------------------------------------------' in line:
            current_stack = []
            continue
        
        # Match something like "  [RDX] <- 0x... : add rdx, 0x10" or "0x... : mov ..."
        match = re.search(r'^(?P<indent>\s*)(?:\[.*\] <- )?0x[0-9a-f]+ : (?P<instr>\w+)', line)
        if match:
            indent = len(match.group('indent'))
            depth = indent // 2
            instr = match.group('instr')
            
            # Maintain the stack for the current branch of the tree
            while current_stack and current_stack[-1][0] >= depth:
                current_stack.pop()
            
            if current_stack:
                parent_instr = current_stack[-1][1]
                # The dependency is: child depends on parent.
                # In the output, the child is shown first (at depth N), and its parents are shown after (at depth N+1)
                # So the line we just matched is the PARENT of the one at depth-1.
                # Wait, the output shows:
                # 0x... : mov (depth 0)
                #   [RAX] <- 0x... : add (depth 1)
                # So 'mov' depends on 'add'.
                pairs[(instr, parent_instr)] += 1
            
            current_stack.append((depth, instr))
            
    return pairs

if __name__ == "__main__":
    filename = 'sniper_llc_output.txt'
    pairs = parse_traces(filename)
    
    print("Most common instruction pairs (Parent, Child) - Child depends on Parent:")
    for pair, count in pairs.most_common(20):
        print(f"{pair[0]:10} -> {pair[1]:10} : {count}")
