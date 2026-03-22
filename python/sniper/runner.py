# python/sniper/runner.py
import os
import subprocess
import sys
from .config import SniperConfig

class SniperRunner:
    def __init__(self, config: SniperConfig):
        self.config = config

    def find_config(self, filename):
        # Search in current dir and then in <root>/config
        paths = [os.getcwd(), os.path.join(self.config.root, 'config')]
        for p in paths:
            full_path = os.path.join(p, filename)
            if os.path.exists(full_path):
                return full_path
            if not filename.endswith('.cfg'):
                full_path += '.cfg'
                if os.path.exists(full_path):
                    return full_path
        return None

    def resolve_config_files(self, filenames):
        resolved = []
        for f in filenames:
            path = self.find_config(f)
            if not path:
                print(f"Error: Cannot find config file {f}", file=sys.stderr)
                sys.exit(1)
            
            # Handle #include recursively
            with open(path, 'r') as fd:
                for line in fd:
                    if line.startswith('#include'):
                        included_file = line.split()[1]
                        resolved.extend(self.resolve_config_files([included_file]))
            
            if path not in resolved:
                resolved.append(path)
        return resolved

    def get_sniper_command(self, ncores=1, configs=None, options=None, trace_prefix=None, output_dir=None):
        sniper_bin = os.path.join(self.config.root, 'lib/sniper')
        if not os.path.exists(sniper_bin):
            sniper_bin = os.path.join(self.config.root, 'bin/sniper') # New CMake path

        cmd = [sniper_bin]
        
        # Base config is always required first
        base_cfg = os.path.join(self.config.root, 'config/base.cfg')
        cmd.extend(['-c', base_cfg])
        
        # Use gainestown as default if no configs provided
        if not configs:
            configs = ['gainestown']
            
        resolved_configs = self.resolve_config_files(configs)
        for cfg_path in resolved_configs:
            if cfg_path != base_cfg: # Don't duplicate base.cfg
                cmd.extend(['-c', cfg_path])
        
        # Use provided output_dir or root
        actual_output_dir = output_dir if output_dir else self.config.root
        
        cmd.extend([
            f'--general/total_cores={ncores}',
            f'--general/output_dir={actual_output_dir}',
        ])
        
        if trace_prefix:
            cmd.extend(['-g', f'--traceinput/trace_prefix={trace_prefix}'])
            cmd.extend(['-g', '--traceinput/enabled=true'])
            
        if options:
            for opt in options:
                cmd.extend(['-g', opt])
                
        return cmd

    def run(self, timeout=None, **kwargs):
        cmd = self.get_sniper_command(**kwargs)

        # Set up environment using the helper in config
        env = self.config.get_env(standalone=True)

        print(f"[SNIPER] Running {' '.join(cmd)}")
        try:
            # Some Sniper output might not be valid UTF-8, use errors='replace'
            proc = subprocess.run(cmd, env=env, timeout=timeout, capture_output=True, text=True, errors='replace')
            if proc.returncode != 0:
                print(f"[SNIPER] Failed with return code {proc.returncode}")
                print(f"[SNIPER] STDOUT: {proc.stdout}")
                print(f"[SNIPER] STDERR: {proc.stderr}")
            return proc
        except subprocess.TimeoutExpired:
            print(f"[SNIPER] Simulation timed out")
            return None
        except Exception as e:
            print(f"[SNIPER] Error running Sniper: {e}")
            return None
