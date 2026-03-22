# python/sniper/run_manager.py
import os
import shutil
import datetime
import json
import yaml

class RunManager:
    def __init__(self, config):
        self.config = config
        self.results_root = os.path.join(self.config.root, 'results')
        if not os.path.exists(self.results_root):
            os.makedirs(self.results_root)

    def create_run(self, name=None):
        timestamp = datetime.datetime.now().strftime('%Y%m%d_%H%M%S')
        if name:
            run_name = f"{timestamp}_{name}"
        else:
            run_name = timestamp
        
        run_dir = os.path.join(self.results_root, run_name)
        os.makedirs(run_dir)
        return run_dir

    def save_metadata(self, run_dir, metadata):
        metadata_file = os.path.join(run_dir, 'metadata.yaml')
        
        # Add system fingerprint
        fingerprint = self._get_system_fingerprint()
        metadata['system_fingerprint'] = fingerprint

        # Convert tuples to lists for better YAML compatibility
        def fix_types(obj):
            if isinstance(obj, tuple):
                return list(obj)
            if isinstance(obj, dict):
                return {k: fix_types(v) for k, v in obj.items()}
            if isinstance(obj, list):
                return [fix_types(i) for i in obj]
            return obj

        fixed_metadata = fix_types(metadata)
        with open(metadata_file, 'w') as f:
            yaml.dump(fixed_metadata, f)

    def _get_system_fingerprint(self):
        import platform
        import hashlib
        import subprocess

        fp = {
            'os': platform.platform(),
            'cpu': platform.processor(),
            'python_version': platform.python_version(),
            'timestamp': datetime.datetime.now().isoformat(),
        }

        # Sniper binary hash
        sniper_bin = os.path.join(self.config.root, 'lib/sniper')
        if os.path.exists(sniper_bin):
            with open(sniper_bin, 'rb') as f:
                fp['sniper_hash'] = hashlib.sha256(f.read()).hexdigest()

        # Git revision
        try:
            git_rev = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=self.config.root).decode().strip()
            fp['git_revision'] = git_rev
        except:
            fp['git_revision'] = self.config.get('git_revision', 'unknown')

        # SDE Version
        try:
            sde_ver = subprocess.check_output(['sde', '--version']).decode().strip().split('\n')[0]
            fp['sde_version'] = sde_ver
        except:
            fp['sde_version'] = 'unknown'

        return fp

    def list_runs(self):
        runs = []
        if not os.path.exists(self.results_root):
            return runs
        
        for d in sorted(os.listdir(self.results_root), reverse=True):
            run_dir = os.path.join(self.results_root, d)
            if os.path.isdir(run_dir):
                metadata_file = os.path.join(run_dir, 'metadata.yaml')
                metadata = {}
                if os.path.exists(metadata_file):
                    try:
                        with open(metadata_file, 'r') as f:
                            metadata = yaml.safe_load(f)
                    except:
                        pass
                runs.append({
                    'name': d,
                    'path': run_dir,
                    'metadata': metadata
                })
        return runs

    def archive_run(self, run_name):
        run_dir = os.path.join(self.results_root, run_name)
        if not os.path.isdir(run_dir):
            print(f"Error: Run {run_name} not found")
            return False
            
        archive_path = os.path.join(self.results_root, f"{run_name}.tar.gz")
        print(f"[ARCHIVE] Creating {archive_path}...")
        
        import tarfile
        with tarfile.open(archive_path, "w:gz") as tar:
            tar.add(run_dir, arcname=run_name)
            
        return archive_path

    def clean_run(self, run_name, keep_stats=True):
        run_dir = os.path.join(self.results_root, run_name)
        if not os.path.isdir(run_dir):
            return False
            
        if keep_stats:
            # Delete large .sift files but keep .sqlite3 and .yaml
            for f in os.listdir(run_dir):
                if f.endswith('.sift') or f.endswith('.log'):
                    os.remove(os.path.join(run_dir, f))
            print(f"[CLEAN] Removed large files from {run_name}")
        else:
            shutil.rmtree(run_dir)
            print(f"[CLEAN] Deleted {run_name}")
            
        return True
