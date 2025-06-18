#!/usr/bin/env python3
# Pretty print runtime_patch_monitor log
# Usage: ./runtime-patch-table.py <trace bin file>
#
# Author: Erkai Yu
import sys

def main(args): 
    global _SCRIPT
    _SCRIPT = args[0]

    if len(args) != 2:
        print(f"Usage: {_SCRIPT} <trace bin file>") 
        return 1

    bin_filename = args[1]
    try:
        with open(bin_filename, mode='rb') as f:
            while True:
                data8 = f.read(8)
                data4 = f.read(4)
                if len(data8) < 8 or len(data4) < 4:
                    break  # reached EOF or partial record
    
                hex8 = int.from_bytes(data8, byteorder='little').to_bytes(8, byteorder='big').hex()
                hex4 = int.from_bytes(data4, byteorder='little').to_bytes(4, byteorder='big').hex()
    
                print(f"0x{hex8} 0x{hex4}")
    
    except FileNotFoundError:
        print(f"Error: File '{filename}' not found.")
    except PermissionError:
        print(f"Error: Permission denied for file '{filename}'.")
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    main(sys.argv)
