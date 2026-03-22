# python/sniper/recorder.py
import os
import subprocess
import sys
from .config import SniperConfig

class SiftRecorder:
    def __init__(self, config: SniperConfig):
        self.config = config

    def get_sde_command(self, arch='intel64', sde_arch=None, tool_args=None, pinball=None):
        sde_home = self.config.get_path('sde_home')
        sde_bin = 'sde64' if arch == 'intel64' else 'sde'
        
        # Use local SDE if available
        if sde_home:
            sde_path = os.path.join(sde_home, sde_bin)
            if os.path.exists(sde_path):
                # We found a local SDE, use it
                pass
            else:
                sde_path = sde_bin # Fallback to path
        else:
            sde_path = sde_bin

        cmd = [sde_path]
        if sde_arch:
            if not sde_arch.startswith('-'):
                sde_arch = '-' + sde_arch
            cmd.append(sde_arch)
        
        # Use the name only, record-trace style
        recorder_so = 'sde_sift_recorder.so'
        cmd.extend(['-t', recorder_so])
        
        if tool_args:
            cmd.extend(tool_args)
            
        if pinball:
            cmd.extend(['-replay', '-replay:basename', pinball])
            
        return cmd

    def run(self, cmdline=None, output_file='trace', arch='intel64', sde_arch=None, pinball=None, verbose=False, 
            roi=False, roi_mpi=False, stop_address=0, emulate_syscalls=False, fast_forward=0, detailed=0, 
            blocksize=0, sift_count_offset=0, response_files=False, pa=False, rtntrace=False, pacsim=False, **kwargs):
        sde_cmd = self.get_sde_command(arch=arch, sde_arch=sde_arch, pinball=pinball)
        
        # Add tool arguments
        tool_args = [
            '-sniper:o', output_file,
            '-sniper:verbose', '1' if verbose else '0',
            '-sniper:roi', '1' if roi else '0',
            '-sniper:roi-mpi', '1' if roi_mpi else '0',
            '-sniper:stop', str(stop_address),
            '-sniper:e', '1' if emulate_syscalls else '0',
            '-sniper:f', str(fast_forward),
            '-sniper:d', str(detailed),
            '-sniper:b', str(blocksize),
            '-sniper:s', str(sift_count_offset),
            '-sniper:r', '1' if response_files else '0',
            '-sniper:pa', '1' if pa else '0',
            '-sniper:rtntrace', '1' if rtntrace else '0',
            '-pacsim', '1' if pacsim else '0',
        ]
        
        cmd_list = sde_cmd + tool_args
        
        if cmdline:
            cmd_list.extend(['--'] + list(cmdline))
        else:
            # Replay mode needs a nullapp
            sde_home = self.config.get('sde_home', '')
            nullapp = os.path.join(sde_home, arch, 'nullapp')
            if not os.path.exists(nullapp):
                nullapp = '/bin/ls' # Fallback
            cmd_list.extend(['--', nullapp])

        if verbose:
            print(f"[SIFT_RECORDER] Running {' '.join(cmd_list)}")
        
        # Use proper environment
        env = self.config.get_env(standalone=False)

        # Run directly without shell=True for security
        proc = subprocess.run(cmd_list, env=env, capture_output=True, text=True, errors='replace')
        if proc.returncode != 0 and verbose:
            print(f"[SIFT_RECORDER] Process failed with return code {proc.returncode}")
            print(f"[SIFT_RECORDER] STDOUT: {proc.stdout}")
            print(f"[SIFT_RECORDER] STDERR: {proc.stderr}")
        return proc

    def record_pinball(self, cmdline, output_file='trace', arch='intel64', sde_arch=None, verbose=True):
        sde_home = self.config.get_path('sde_home')
        sde_bin = 'sde64' if arch == 'intel64' else 'sde'
        sde_path = os.path.join(sde_home, sde_bin) if sde_home else sde_bin

        cmd = [sde_path]
        if sde_arch:
            if not sde_arch.startswith('-'):
                sde_arch = '-' + sde_arch
            cmd.append(sde_arch)
        
        cmd.extend(['-log', '-log:basename', output_file, '--'])
        cmd.extend(list(cmdline))

        if verbose:
            print(f"[SDE_LOG] Running {' '.join(cmd)}")
        
        env = self.config.get_env(standalone=False)
        return subprocess.run(cmd, env=env)

    def replay_pinball_to_sift(self, pinball, output_file='trace', arch='intel64', sde_arch=None, verbose=True):
        # We use pinbin directly from the SDE kit to avoid the -work-dir issue
        sde_home = self.config.get_path('sde_home')
        if not sde_home:
            print("Error: sde_home not found in config")
            return None
            
        pin_bin = os.path.join(sde_home, arch, 'pinbin')
        recorder_so = os.path.join(sde_home, arch, 'sde_sift_recorder.so')
        
        # Need to set up LD_LIBRARY_PATH for the SDE libraries
        env = self.config.get_env(standalone=False)
        sde_libs = [
            os.path.join(sde_home, arch, 'xed_lib'),
            os.path.join(sde_home, arch, 'pin_lib'),
            os.path.join(sde_home, 'pinkit', arch, 'runtime/pincrt')
        ]
        extra_path = ':'.join(sde_libs)
        env['LD_LIBRARY_PATH'] = extra_path + (':' + env['LD_LIBRARY_PATH'] if 'LD_LIBRARY_PATH' in env else '')

        cmd = [
            pin_bin,
            '-t', recorder_so,
            '-sniper:o', output_file,
            '-sniper:r', '0', # No response files needed for offline conversion
            '-replay',
            '-replay:basename', pinball,
            '--',
            os.path.join(sde_home, arch, 'nullapp')
        ]

        if verbose:
            print(f"[PINBALL_REPLAY] Converting {pinball} to SIFT...")
            print(f"[PINBALL_REPLAY] Running {' '.join(cmd)}")
            
        return subprocess.run(cmd, env=env)
        
def record(cmdline, output_file, arch='intel64', sde_arch=None):
    config = SniperConfig()
    recorder = SiftRecorder(config)
    # Placeholder for the actual execution logic
    print(f"Recording {cmdline} to {output_file}...")
