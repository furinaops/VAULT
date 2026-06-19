#!/usr/bin/env python3
import subprocess
import os
import sys

def main():
    root_dir = os.path.dirname(os.path.abspath(__file__))
    receiver_dir = os.path.join(root_dir, "build", "receiver")
    binary = os.path.join(receiver_dir, "cp_receiver")
    config = os.path.join(root_dir, "factory.cpproj")
    
    if not os.path.exists(binary):
        print(f"Error: Binary not found at {binary}")
        print("Please run 'mkdir -p build && cd build && cmake .. && make' first.")
        sys.exit(1)
        
    # Ensure config is in the receiver directory
    dest_config = os.path.join(receiver_dir, "factory.cpproj")
    if not os.path.exists(dest_config) or os.path.getmtime(config) > os.path.getmtime(dest_config):
        import shutil
        shutil.copy2(config, dest_config)
    
    try:
        print("\n" + "="*50)
        print("      CP PROTOCOL SYSTEM - RECEIVER MODULE")
        print("="*50 + "\n")
        
        # The C++ app asks for the key on stdin, subprocess.run will handle this naturally
        subprocess.run([binary], cwd=receiver_dir, check=True)
    except KeyboardInterrupt:
        print("\n[!] Receiver stopped by user.")
    except subprocess.CalledProcessError as e:
        print(f"\n[!] Error: Receiver exited with code {e.returncode}")

if __name__ == "__main__":
    main()
