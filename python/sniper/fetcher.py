import os
import requests
import tarfile
import shutil
import yaml
from .config import SniperConfig

class DependencyFetcher:
    DEPENDENCIES = {
        'pin': {
            'url': 'https://snipersim.org/packages/pin-external-3.31-98869-gfa6f126a8-gcc-linux.tar.gz',
            'dir': 'pin_kit'
        },
        'sde': {
            'url': 'https://downloadmirror.intel.com/913594/sde-external-10.7.0-2026-02-18-lin.tar.xz',
            'dir': 'sde_kit'
        },
        'pinplay': {
            'url': 'https://snipersim.org/packages/pinplay-dcfg-3.11-pin-3.11-97998-g7ecce2dac-gcc-linux.tar.bz2',
            'dir': 'pinplay_kit'
        }
    }

    def __init__(self, config: SniperConfig):
        self.config = config

    def download_and_extract(self, name):
        if name not in self.DEPENDENCIES:
            print(f"Unknown dependency: {name}")
            return False

        dep = self.DEPENDENCIES[name]
        url = dep['url']
        target_dir = os.path.join(self.config.root, dep['dir'])
        
        filename = url.split('/')[-1]
        filepath = os.path.join(self.config.root, filename)

        if not os.path.exists(target_dir):
            print(f"Downloading {name} from {url}...")
            response = requests.get(url, stream=True)
            if response.status_code == 200:
                with open(filepath, 'wb') as f:
                    for chunk in response.iter_content(chunk_size=8192):
                        f.write(chunk)
                
                print(f"Extracting {filename} to {dep['dir']}...")
                # SDE kits often have a nested directory
                with tarfile.open(filepath) as tar:
                    # Find the top-level directory in the tarball
                    top_dir = tar.getnames()[0].split('/')[0]
                    tar.extractall(path=self.config.root)
                    
                    # Rename the extracted directory to our standard name
                    extracted_path = os.path.join(self.config.root, top_dir)
                    if os.path.exists(target_dir):
                        shutil.rmtree(target_dir)
                    os.rename(extracted_path, target_dir)
                
                os.remove(filepath)
                return True
            else:
                print(f"Failed to download {name}: HTTP {response.status_code}")
                return False
        else:
            print(f"{name} already exists at {target_dir}")
            return True

    def update_config(self, name):
        dep = self.DEPENDENCIES[name]
        target_dir = dep['dir']
        
        yaml_path = os.path.join(self.config.root, 'config/sniper.yaml')
        config_data = {}
        if os.path.exists(yaml_path):
            with open(yaml_path, 'r') as f:
                config_data = yaml.safe_load(f) or {}
        
        key = f"{name}_home"
        config_data[key] = target_dir
        
        # Special case for SDE kit's pinkit
        if name == 'sde':
            config_data['pin_home'] = os.path.join(target_dir, 'pinkit')

        with open(yaml_path, 'w') as f:
            yaml.dump(config_data, f)
        print(f"Updated {yaml_path} with {key}={target_dir}")
