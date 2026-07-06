#!/usr/bin/env python3
"""
whisperd listener stub - copy this to build your peripheral
Listens to the clock and prints every tick.
"""

import socket
import json
import sys
import os

WHISPER_SOCKET = "/tmp/whisperd.sock"
WHISPER_DEADLINE_SECONDS = 86400

class WhisperListener:
    def __init__(self, socket_path=WHISPER_SOCKET):
        self.socket_path = socket_path
        self.sock = None
        
    def connect(self):
        """Connect to whisperd's Unix socket"""
        if not os.path.exists(self.socket_path):
            print(f"Error: {self.socket_path} not found. Is whisperd running?")
            sys.exit(1)
            
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.sock.connect(self.socket_path)
        self.sock.setblocking(True)
        print(f"[listener] Connected to {self.socket_path}")
        
    def listen(self):
        """Block and process every tick"""
        buffer = ""
        while True:
            data = self.sock.recv(1024).decode()
            if not data:
                break
            buffer += data
            while "\n" in buffer:
                line, buffer = buffer.split("\n", 1)
                if line.strip():
                    whisper = json.loads(line)
                    self.on_tick(whisper)
                    
    def on_tick(self, whisper):
        """Override this method with your peripheral logic"""
        tick = whisper['tick']
        dead = whisper['dead']
        remaining = (WHISPER_DEADLINE_SECONDS - (whisper['ns'] / 1e9)) if not dead else 0
        
        if tick % 100 == 0:  # Print every 100th tick
            print(f"[tick {tick}] dead={dead}, remaining={remaining:.1f}s")
        
    def run(self):
        try:
            self.connect()
            self.listen()
        except KeyboardInterrupt:
            print("\n[listener] stopping")
        finally:
            if self.sock:
                self.sock.close()

class MemoryLogger(WhisperListener):
    """Example peripheral: logs every tick to JSONL"""
    def __init__(self, log_path="/tmp/whisper_memory.jsonl"):
        super().__init__()
        self.log_path = log_path
        
    def on_tick(self, whisper):
        with open(self.log_path, 'a') as f:
            f.write(json.dumps(whisper) + '\n')
        if whisper['tick'] % 1000 == 0:
            print(f"[memory] logged {whisper['tick']} ticks")

class SensorSimulator(WhisperListener):
    """Example peripheral: simulates sensor readings"""
    def on_tick(self, whisper):
        if whisper['tick'] % 10 == 0:
            import random
            sensor_data = {
                'tick': whisper['tick'],
                'temperature': random.uniform(20, 50),
                'battery': random.uniform(0.5, 1.0),
                'anchor_status': 'SATISFIED' if random.random() > 0.1 else 'VIOLATED'
            }
            print(f"[sensor] {sensor_data}")

if __name__ == "__main__":
    # Default: just print ticks
    # To use a peripheral: uncomment one of these:
    # listener = MemoryLogger()
    # listener = SensorSimulator()
    listener = WhisperListener()
    listener.run()
