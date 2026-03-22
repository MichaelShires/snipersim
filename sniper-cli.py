#!/usr/bin/env python3
import sys
import os

# Add the project root to the path so we can import the sniper package
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), 'python'))

from sniper.main import cli

if __name__ == '__main__':
    cli()
