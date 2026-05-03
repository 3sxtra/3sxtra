import os
import re

def process_game_state_h():
    path = "d:/3sxtra/src/include/game_state.h"
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Replace typedef struct State { ... } State;
    content = content.replace("typedef struct State {", "typedef struct RollbackState {")
    content = content.replace("} State;", "} RollbackState;")
    content = content.replace("void load_state(const struct State* src);", "void load_state(const struct RollbackState* src);")
    
    with open(path, 'w', encoding='utf-8') as f:
        f.write(content)

def process_game_state_c():
    path = "d:/3sxtra/src/netplay/game_state.c"
    with open(path, 'r', encoding='utf-8') as f:
        lines = f.readlines()
        
    for i, line in enumerate(lines):
        if "sanitize_work" in line:
            continue
        if "es->frw" in line:
            continue
        if "(State_Other*)" in line:
            continue
        if "Release_Effect(" in line:
            continue
            
        # specifically matching rollback state signatures:
        if "state_buffer" in line:
            lines[i] = line.replace("State", "RollbackState")
        elif "gather_state(State*" in line:
            lines[i] = line.replace("State*", "RollbackState*")
        elif "note_state(const State" in line:
            lines[i] = line.replace("State*", "RollbackState*").replace("const State", "const RollbackState")
        elif "sizeof(State)" in line:
            lines[i] = line.replace("sizeof(State)", "sizeof(RollbackState)")
        elif "load_state(const State*" in line:
            lines[i] = line.replace("State*", "RollbackState*")
        elif "(State*)buffer" in line:
            lines[i] = line.replace("State*", "RollbackState*")
        elif "(State*)event->data.load.state" in line:
            lines[i] = line.replace("State*", "RollbackState*")
            
    with open(path, 'w', encoding='utf-8') as f:
        f.writelines(lines)

def process_netplay_c():
    path = "d:/3sxtra/src/netplay/netplay.c"
    with open(path, 'r', encoding='utf-8') as f:
        lines = f.readlines()
        
    for i, line in enumerate(lines):
        if "sizeof(State)" in line:
            lines[i] = line.replace("sizeof(State)", "sizeof(RollbackState)")
            
    with open(path, 'w', encoding='utf-8') as f:
        f.writelines(lines)

if __name__ == "__main__":
    process_game_state_h()
    process_game_state_c()
    process_netplay_c()
    print("Done")
