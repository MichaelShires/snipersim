import os
import itertools
import subprocess
import time
from concurrent.futures import ProcessPoolExecutor
from .runner import SniperRunner
from .run_manager import RunManager

class SlurmProvider:
    def __init__(self, sweep_manager):
        self.sm = sweep_manager

    def submit_task(self, task):
        params, trace_prefix, name = task
        run_name = f"sweep_{name}"
        run_dir = self.sm.run_manager.create_run(run_name)
        
        metadata = {
            'type': 'sweep',
            'scheduler': 'slurm',
            'params': params,
            'trace_prefix': trace_prefix
        }
        self.sm.run_manager.save_metadata(run_dir, metadata)

        # Generate slurm script
        script_path = os.path.join(run_dir, 'slurm_job.sh')
        
        options = []
        for key, value in params.items():
            options.append(f'--{key}={value}')
        
        # Build command line for sniper-cli.py run
        cmd = [
            'python3', 
            os.path.join(self.sm.config.root, 'sniper-cli.py'),
            'run',
            '--trace-prefix', trace_prefix,
            '--name', run_name,
            '--output-dir', run_dir # Need to ensure run command supports this
        ]
        for opt in options:
            cmd.extend(['-g', opt])

        with open(script_path, 'w') as f:
            f.write("#!/bin/bash\n")
            f.write(f"#SBATCH --job-name=sniper_{name}\n")
            f.write(f"#SBATCH --output={run_dir}/slurm.out\n")
            f.write(f"#SBATCH --error={run_dir}/slurm.err\n")
            f.write(f"#SBATCH --ntasks=1\n")
            f.write(f"#SBATCH --cpus-per-task=1\n")
            f.write(f"#SBATCH --mem=4G\n")
            f.write(f"#SBATCH --time=01:00:00\n\n")
            
            # Inject environment setup if needed
            env = self.sm.config.get_env(standalone=True)
            for k, v in env.items():
                f.write(f"export {k}=\"{v}\"\n")
            
            f.write("\n")
            f.write(" ".join(cmd) + "\n")

        print(f"[SLURM] Submitting job: {run_name}")
        res = subprocess.run(['sbatch', script_path], capture_output=True, text=True)
        if res.returncode == 0:
            print(f"[SLURM] {res.stdout.strip()}")
            return run_name
        else:
            print(f"[SLURM] Error: {res.stderr}")
            return None

class SweepManager:
    def __init__(self, config):
        self.config = config
        self.runner = SniperRunner(config)
        self.run_manager = RunManager(config)

    def run_simulation(self, task):
        params, trace_prefix, name = task
        options = []
        for key, value in params.items():
            options.append(f'--{key}={value}')
        
        run_name = f"sweep_{name}"
        run_dir = self.run_manager.create_run(run_name)
        
        metadata = {
            'type': 'sweep',
            'scheduler': 'local',
            'params': params,
            'trace_prefix': trace_prefix
        }
        self.run_manager.save_metadata(run_dir, metadata)
        
        print(f"[SWEEP] Starting: {run_name} with {params}")
        self.runner.run(trace_prefix=trace_prefix, output_dir=run_dir, options=options)
        return run_name

    def run_sweep(self, trace_prefix, sweep_config, parallel=4, scheduler='local'):
        # sweep_config example: {'perf_model/l3_cache/cache_size': [1024, 2048, 4096]}
        keys = list(sweep_config.keys())
        values = list(sweep_config.values())
        
        combinations = list(itertools.product(*values))
        tasks = []
        
        for i, combo in enumerate(combinations):
            params = dict(zip(keys, combo))
            name = "_".join([f"{k.split('/')[-1]}{v}" for k, v in params.items()])
            tasks.append((params, trace_prefix, name))
            
        print(f"[SWEEP] Generated {len(tasks)} simulation tasks using {scheduler} scheduler")
        
        if scheduler == 'slurm':
            provider = SlurmProvider(self)
            results = []
            for task in tasks:
                results.append(provider.submit_task(task))
            return results
        else:
            with ProcessPoolExecutor(max_workers=parallel) as executor:
                results = list(executor.map(self.run_simulation, tasks))
            return results
