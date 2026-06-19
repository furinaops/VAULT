#!/usr/bin/env python3
import subprocess
import os
import sys

def main():
    root_dir = os.path.dirname(os.path.abspath(__file__))
    sender_dir = os.path.join(root_dir, "build", "sender")
    binary = os.path.join(sender_dir, "cp_sender")
    config = os.path.join(root_dir, "factory.cpproj")
    
    if not os.path.exists(binary):
        print(f"Error: Binary not found at {binary}")
        print("Please run 'mkdir -p build && cd build && cmake .. && make' first.")
        sys.exit(1)
        
    # Ensure config is in the sender directory
    dest_config = os.path.join(sender_dir, "factory.cpproj")
    if not os.path.exists(dest_config) or os.path.getmtime(config) > os.path.getmtime(dest_config):
        import shutil
        shutil.copy2(config, dest_config)
    
    config_data = load_config_summary(config)
    
    try:
        print("\n" + "="*50)
        print("      CP PROTOCOL SYSTEM - SENDER MODULE")
        print("="*50)
        print(f"Server Host: {config_data['server']['host']}")
        print(f"Server Port: {config_data['server']['port']}")
        print("="*50 + "\n")
        
        # We use subprocess.Popen to allow for better control if we wanted, 
        # but for a simple "chat UI" that passes through to the C++ binary, 
        # subprocess.run is sufficient as it connects stdin/stdout.
        subprocess.run([binary], cwd=sender_dir, check=True)
    except KeyboardInterrupt:
        print("\n[!] Sender stopped by user.")
    except subprocess.CalledProcessError as e:
        print(f"\n[!] Error: Sender exited with code {e.returncode}")

def load_config_summary(config_path):
    # Simple parser to get host/port for the header without requiring yaml lib
    config = {'server': {'host': 'unknown', 'port': 'unknown'}}
    try:
        with open(config_path, 'r') as f:
            lines = f.readlines()
            for i, line in enumerate(lines):
                if 'server:' in line:
                    for j in range(i+1, i+3):
                        if 'host:' in lines[j]:
                            config['server']['host'] = lines[j].split(':')[1].strip().strip('"')
                        if 'port:' in lines[j]:
                            config['server']['port'] = lines[j].split(':')[1].strip()
    except:
        pass
    return config

if __name__ == "__main__":
    main()
