# python/sniper/config.py
import os
import sys
import yaml

class SniperConfig:
    def __init__(self, sniper_root=None):
        if sniper_root:
            self.root = os.path.abspath(sniper_root)
        elif 'SNIPER_ROOT' in os.environ:
            self.root = os.path.abspath(os.environ['SNIPER_ROOT'])
        else:
            # Assume we are in <root>/python/sniper/
            self.root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
        
        self.data = {}
        self._load_config()

    def _load_config(self):
        # 1. Load low-level auto-generated config (safely parse key=value)
        py_config = os.path.join(self.root, 'config/sniper.py')
        if os.path.isfile(py_config):
            try:
                with open(py_config, "r") as f:
                    for line in f:
                        line = line.strip()
                        if not line or line.startswith('#'):
                            continue
                        if '=' in line:
                            key, value = line.split('=', 1)
                            # Remove quotes if present
                            value = value.strip().strip('"').strip("'")
                            self.data[key.strip()] = value
            except Exception as e:
                print(f"Warning: Failed to load {py_config}: {e}", file=sys.stderr)

        # 2. Load user-level YAML config
        yaml_config = os.path.join(self.root, 'config/sniper.yaml')
        if os.path.isfile(yaml_config):
            try:
                with open(yaml_config, 'r') as f:
                    user_data = yaml.safe_load(f)
                    if user_data:
                        self.data.update(user_data)
            except Exception as e:
                print(f"Warning: Failed to load {yaml_config}: {e}", file=sys.stderr)

        # 3. Load user-specific config from home directory
        home_config = os.path.expanduser('~/.sniper.yaml')
        if os.path.isfile(home_config):
            try:
                with open(home_config, 'r') as f:
                    home_data = yaml.safe_load(f)
                    if home_data:
                        self.data.update(home_data)
            except Exception as e:
                print(f"Warning: Failed to load {home_config}: {e}", file=sys.stderr)

        # Convert relative paths to absolute
        for key in ['sde_home', 'pin_home', 'xed_home', 'torch_home', 'dynamorio_home', 'clang_path']:
            if key in self.data:
                path = self.data[key]
                if path and not os.path.isabs(path):
                    self.data[key] = os.path.abspath(os.path.join(self.root, path))
        
        # Default clang path if not set
        if 'clang_path' not in self.data:
            default_clang = os.path.expanduser('~/opt/llvm-project/build/bin/clang')
            if os.path.exists(default_clang):
                self.data['clang_path'] = default_clang

    def get(self, key, default=None):
        return self.data.get(key, default)

    def get_path(self, key):
        path = self.get(key)
        if path and os.path.exists(path):
            return path
        
        # Special case for sde_home if it's on the path
        if key == 'sde_home':
            import shutil
            sde_bin = shutil.which('sde')
            if sde_bin:
                return os.path.dirname(os.path.dirname(sde_bin))
        return None

    def get_env(self, standalone=False):
        """Get the proper environment variables for running Sniper/SDE"""
        tools_path = os.path.join(self.root, 'tools')
        if tools_path not in sys.path:
            sys.path.append(tools_path)
        
        import run_sniper
        pin_home = self.get_path('pin_home')
        arch = self.get('target', 'intel64')
        xed_home = self.get_path('xed_home')
        torch_home = self.get_path('torch_home')
        
        env = run_sniper.setup_env(self.root, pin_home, arch, standalone, xed_home, torch_home)
        
        # Add local libs if they exist
        local_libs = []
        if xed_home:
            xed_lib = os.path.join(xed_home, 'lib')
            if os.path.exists(xed_lib): local_libs.append(xed_lib)
        if torch_home:
            torch_lib = os.path.join(torch_home, 'lib')
            if os.path.exists(torch_lib): local_libs.append(torch_lib)
        
        if local_libs:
            extra_path = ':'.join(local_libs)
            if 'LD_LIBRARY_PATH' in env:
                env['LD_LIBRARY_PATH'] = extra_path + ':' + env['LD_LIBRARY_PATH']
            else:
                env['LD_LIBRARY_PATH'] = extra_path
                
        return env

    def validate(self):
        """Validate the environment and dependencies"""
        results = []
        # Check root
        if not os.path.isdir(self.root):
            results.append(('Root', 'FAIL', f"Sniper root directory {self.root} does not exist"))
        else:
            results.append(('Root', 'OK', self.root))

        # Check binary
        sniper_bin = os.path.join(self.root, 'lib/sniper')
        if not os.path.exists(sniper_bin):
            sniper_bin = os.path.join(self.root, 'bin/sniper')
        
        if os.path.exists(sniper_bin):
            results.append(('Sniper Bin', 'OK', sniper_bin))
        else:
            results.append(('Sniper Bin', 'FAIL', "Sniper binary not found (run make)"))

        # Check dependencies
        for dep in ['sde_home', 'pin_home', 'xed_home']:
            path = self.get_path(dep)
            if path:
                results.append((dep, 'OK', path))
            else:
                results.append((dep, 'FAIL', f"Missing {dep} in config or path does not exist"))

        # Check pin-g++
        pin_home = self.get_path('pin_home')
        if pin_home:
            arch = self.get('target', 'intel64')
            pingpp = os.path.join(pin_home, arch, 'pinrt/bin/pin-g++')
            if os.path.exists(pingpp):
                results.append(('pin-g++', 'OK', pingpp))
            else:
                results.append(('pin-g++', 'FAIL', f"pin-g++ not found at {pingpp}"))

        return results
