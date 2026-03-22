# python/sniper/main.py
import click
import os
import sys
import tempfile
import threading
import datetime
import shutil
import subprocess
from .config import SniperConfig
from .recorder import SiftRecorder
from .runner import SniperRunner
from .stats import SniperStats
from .run_manager import RunManager
from .fetcher import DependencyFetcher
from .sweep_manager import SweepManager
from .config_validator import ConfigValidator

try:
    from rich.console import Console
    from rich.table import Table
    from rich.progress import Progress, SpinnerColumn, TextColumn
    console = Console()
except ImportError:
    console = None

@click.group()
@click.option('--root', type=click.Path(exists=True), help='Sniper root directory')
@click.pass_context
def cli(ctx, root):
    """Sniper Simulation Modern CLI"""
    ctx.obj = SniperConfig(root)

@cli.command()
@click.argument('dependency', type=click.Choice(['pin', 'sde', 'pinplay', 'all']), default='all')
@click.pass_obj
def fetch(config, dependency):
    """Fetch and configure dependencies (pin, sde, pinplay)"""
    fetcher = DependencyFetcher(config)
    deps = ['pin', 'sde', 'pinplay'] if dependency == 'all' else [dependency]
    
    for dep in deps:
        if console:
            with Progress(
                SpinnerColumn(),
                TextColumn("[progress.description]{task.description}"),
                transient=True,
            ) as progress:
                progress.add_task(description=f"Fetching {dep}...", total=None)
                success = fetcher.download_and_extract(dep)
        else:
            success = fetcher.download_and_extract(dep)
            
        if success:
            fetcher.update_config(dep)
            if console:
                console.print(f"[green]Successfully fetched and configured {dep}[/green]")

@cli.group()
@click.pass_obj
def runs(config):
    """Manage simulation runs"""
    pass

@runs.command(name='list')
@click.pass_obj
def list_runs(config):
    """List simulation runs"""
    manager = RunManager(config)
    runs_list = manager.list_runs()
    
    if console:
        table = Table(title="Sniper Simulation Runs")
        table.add_column("Run Name", style="cyan")
        table.add_column("Command", style="magenta")
        table.add_column("Architecture", style="green")
        
        for run in runs_list:
            metadata = run.get('metadata', {})
            cmd = " ".join(metadata.get('cmdline', [])) if isinstance(metadata.get('cmdline'), (list, tuple)) else str(metadata.get('cmdline', ''))
            if not cmd and metadata.get('pinball'):
                cmd = f"Pinball: {metadata['pinball']}"
            arch = metadata.get('arch', 'N/A')
            table.add_row(run['name'], cmd[:50] + ("..." if len(cmd) > 50 else ""), arch)
        
        console.print(table)
    else:
        print(f"{'Run Name':30} | {'Command':50} | {'Arch'}")
        print("-" * 100)
        for run in runs_list:
            metadata = run.get('metadata', {})
            cmd = " ".join(metadata.get('cmdline', [])) if isinstance(metadata.get('cmdline'), (list, tuple)) else str(metadata.get('cmdline', ''))
            if not cmd and metadata.get('pinball'):
                cmd = f"Pinball: {metadata['pinball']}"
            arch = metadata.get('arch', 'N/A')
            print(f"{run['name']:30} | {cmd[:50]:50} | {arch}")

@runs.command(name='stats')
@click.argument('run_name')
@click.option('--filter', 'filter_str', help='Filter stats by string')
@click.pass_obj
def run_stats(config, run_name, filter_str):
    """Show stats for a specific run"""
    manager = RunManager(config)
    run_path = os.path.join(manager.results_root, run_name)
    if not os.path.isdir(run_path):
        print(f"Error: Run {run_name} not found in {manager.results_root}")
        return

    stats_tool = SniperStats(config)
    data = stats_tool.get_stats(run_path)
    if not data:
        print("No stats found for this run (is it finished?)")
        return

    if console:
        table = Table(title=f"Stats for {run_name}")
        table.add_column("Metric", style="cyan")
        table.add_column("Value", style="magenta")
        
        results = data.get('results', {})
        keys = sorted(results.keys())
        if not filter_str:
            interesting = ['core.instructions', 'core.cycles', 'core.cpi', 'L3.misses', 'dram.reads', 'dram.writes']
            keys = [k for k in keys if any(i in k for i in interesting)]
        else:
            keys = [k for k in keys if filter_str in k]

        for key in keys:
            table.add_row(key, str(results[key]))
        
        console.print(table)
    else:
        stats_tool.dump(run_path, filter_str)

@runs.command(name='report')
@click.argument('pattern', default='*')
@click.option('--metrics', '-m', multiple=True, help='Metrics to include in report')
@click.option('--output', '-o', type=click.Choice(['table', 'csv', 'json']), default='table')
@click.pass_obj
def runs_report(config, pattern, metrics, output):
    """Generate an aggregate report for multiple runs"""
    from .report_manager import ReportManager
    rm = ReportManager(config)
    rm.generate_report(pattern, metrics, output)

@runs.command(name='archive')
@click.argument('run_name')
@click.pass_obj
def runs_archive(config, run_name):
    """Archive a simulation run into a .tar.gz file"""
    manager = RunManager(config)
    manager.archive_run(run_name)

@runs.command(name='clean')
@click.argument('run_name', default='*')
@click.option('--all', 'delete_all', is_flag=True, help='Delete the entire directory instead of just traces')
@click.pass_obj
def runs_clean(config, run_name, delete_all):
    """Clean up simulation runs (removes large trace files)"""
    manager = RunManager(config)
    import fnmatch
    runs = manager.list_runs()
    for run in runs:
        if fnmatch.fnmatch(run['name'], run_name):
            manager.clean_run(run['name'], keep_stats=not delete_all)

@cli.command()
@click.pass_obj
def doctor(config):
    """Check environment and dependencies"""
    results = config.validate()
    
    if console:
        table = Table(title="Sniper Doctor Report")
        table.add_column("Component", style="cyan")
        table.add_column("Status", style="bold")
        table.add_column("Details", style="magenta")
        
        for component, status, details in results:
            status_style = "green" if status == "OK" else "red"
            table.add_row(component, f"[{status_style}]{status}[/{status_style}]", details)
        
        console.print(table)
    else:
        print("Sniper Doctor Report")
        print("-" * 20)
        for component, status, details in results:
            print(f"{component:15} | {status:4} | {details}")

@cli.command()
@click.argument('cmdline', nargs=-1)
@click.option('-o', '--output', default='trace', help='Output SIFT trace basename')
@click.option('-a', '--arch', default='intel64', help='Target architecture')
@click.option('--sde-arch', help='SDE chip architecture (e.g., -dmr)')
@click.option('--pinball', help='Input pinball basename')
@click.option('-v', '--verbose', is_flag=True, help='Verbose output')
@click.pass_obj
def record(config, cmdline, output, arch, sde_arch, pinball, verbose):
    """Record a SIFT trace using SDE/Pin"""
    recorder = SiftRecorder(config)
    if pinball:
        recorder.replay_pinball_to_sift(pinball=pinball, output_file=output, arch=arch, sde_arch=sde_arch, verbose=verbose)
    else:
        recorder.run(cmdline=cmdline, output_file=output, arch=arch, sde_arch=sde_arch, pinball=pinball, verbose=verbose)

@cli.command(name='record-pinball')
@click.argument('cmdline', nargs=-1)
@click.option('-o', '--output', default='trace', help='Output pinball basename')
@click.option('-a', '--arch', default='intel64', help='Target architecture')
@click.option('--sde-arch', help='SDE chip architecture (e.g., -dmr)')
@click.pass_obj
def record_pinball(config, cmdline, output, arch, sde_arch):
    """Record an SDE pinball"""
    recorder = SiftRecorder(config)
    recorder.record_pinball(cmdline=cmdline, output_file=output, arch=arch, sde_arch=sde_arch)

@cli.command()
@click.option('-n', '--ncores', default=1, help='Number of cores')
@click.option('-c', '--config', 'configs', multiple=True, help='Configuration files')
@click.option('-g', '--option', 'options', multiple=True, help='Override configuration options')
@click.option('--trace-prefix', help='Prefix for SIFT trace files')
@click.option('--name', help='Optional name for the simulation run')
@click.option('--output-dir', help='Explicit output directory')
@click.option('-v', '--verbose', is_flag=True, help='Verbose output')
@click.pass_obj
def run(config_obj, ncores, configs, options, trace_prefix, name, output_dir, verbose):
    """Run the Sniper simulator"""
    # Pre-flight validation
    validator = ConfigValidator(config_obj.root)
    issues = validator.validate_options(options)
    for level, msg in issues:
        if console:
            color = "red" if level == "ERROR" else "yellow"
            console.print(f"[[{color}]{level}[/{color}]] {msg}")
        else:
            print(f"[{level}] {msg}")
        if level == "ERROR": return

    if output_dir:
        run_dir = output_dir
        if not os.path.exists(run_dir):
            os.makedirs(run_dir)
    else:
        manager = RunManager(config_obj)
        run_dir = manager.create_run(name)
    
    # Save metadata
    metadata = {
        'type': 'run',
        'ncores': ncores,
        'configs': list(configs),
        'trace_prefix': trace_prefix,
        'timestamp': datetime.datetime.now().isoformat(),
    }
    # Only use manager if we are in results root
    if not output_dir:
        manager.save_metadata(run_dir, metadata)
    
    runner = SniperRunner(config_obj)
    proc = runner.run(ncores=ncores, configs=configs, options=options, trace_prefix=trace_prefix, output_dir=run_dir)
    
    if verbose and proc:
        print(f"[SNIPER] STDOUT: {proc.stdout}")
        print(f"[SNIPER] STDERR: {proc.stderr}")
        
    print(f"\n[SNIPER] Run complete. Results saved in: {run_dir}")

@cli.command()
@click.argument('stats_file', type=click.Path(exists=True), required=False)
@click.option('--filter', 'filter_str', help='Filter stats by string')
@click.pass_obj
def stats(config, stats_file, filter_str):
    """Dump simulation statistics"""
    stats_tool = SniperStats(config)
    stats_tool.dump(stats_file=stats_file, filter_str=filter_str)

@cli.command()
@click.argument('cmdline', nargs=-1)
@click.option('-n', '--ncores', default=1, help='Number of cores')
@click.option('-c', '--config', 'configs', multiple=True, help='Configuration files')
@click.option('-a', '--arch', default='intel64', help='Target architecture')
@click.option('--sde-arch', help='SDE chip architecture (e.g., -dmr)')
@click.option('--pinball', help='Input pinball basename')
@click.option('--name', help='Optional name for the simulation run')
@click.option('-v', '--verbose', is_flag=True, help='Verbose output')
@click.pass_obj
def sim(config_obj, cmdline, ncores, configs, arch, sde_arch, pinball, name, verbose):
    """Full simulation: record and run in one step"""
    manager = RunManager(config_obj)
    run_dir = manager.create_run(name)
    
    trace_prefix = os.path.join(run_dir, 'trace')
    
    # Save metadata
    metadata = {
        'cmdline': cmdline,
        'ncores': ncores,
        'configs': list(configs),
        'arch': arch,
        'sde_arch': sde_arch,
        'pinball': pinball,
        'timestamp': datetime.datetime.now().isoformat(),
    }
    manager.save_metadata(run_dir, metadata)
    
    recorder = SiftRecorder(config_obj)
    runner = SniperRunner(config_obj)
    
    try:
        if pinball:
            print(f"[SIFT_RECORDER] Replaying pinball {pinball}")
            proc = recorder.replay_pinball_to_sift(pinball=pinball, output_file=trace_prefix, arch=arch, sde_arch=sde_arch, verbose=True)
        else:
            print(f"[SIFT_RECORDER] Starting live recorder for {cmdline}")
            proc = recorder.run(cmdline=cmdline, output_file=trace_prefix, arch=arch, sde_arch=sde_arch, pinball=pinball, verbose=True)
            
        if proc.returncode != 0:
            print(f"[SIFT_RECORDER] Failed with return code {proc.returncode}")
            return
    except Exception as e:
        print(f"[SIFT_RECORDER] Error: {e}")
        return

    default_options = [
        '--traceinput/emulate_syscalls=false',
        '--traceinput/stop_with_first_app=true',
        '--traceinput/restart_apps=false',
        '--traceinput/num_apps=1',
    ]
    runner.run(ncores=ncores, configs=configs, trace_prefix=trace_prefix, 
               output_dir=run_dir, options=default_options)
    
    print(f"\n[SNIPER] Simulation complete. Results saved in: {run_dir}")

@cli.command()
@click.argument('trace_prefix')
@click.option('--config', 'sweep_config_file', type=click.Path(exists=True), required=True, help='YAML file with sweep parameters')
@click.option('-j', '--parallel', default=4, help='Number of parallel simulations')
@click.option('--scheduler', type=click.Choice(['local', 'slurm']), default='local', help='Scheduler to use')
@click.pass_obj
def sweep(config, trace_prefix, sweep_config_file, parallel, scheduler):
    """Run a parameter sweep from a YAML config"""
    import yaml
    with open(sweep_config_file, 'r') as f:
        sweep_data = yaml.safe_load(f)
    
    manager = SweepManager(config)
    manager.run_sweep(trace_prefix, sweep_data, parallel=parallel, scheduler=scheduler)

@cli.command()
@click.option('--clean', is_flag=True, help='Clean before building')
@click.option('--asan', is_flag=True, help='Enable AddressSanitizer')
@click.option('--ubsan', is_flag=True, help='Enable UndefinedBehaviorSanitizer')
@click.option('-D', 'defines', multiple=True, help='Additional CMake defines')
@click.pass_obj
def build(config, clean, asan, ubsan, defines):
    """Build Sniper using CMake and Make"""
    if clean:
        print("[BUILD] Cleaning...")
        subprocess.run(['make', 'clean'], cwd=config.root)
        if os.path.exists(os.path.join(config.root, 'build')):
            shutil.rmtree(os.path.join(config.root, 'build'))

    print("[BUILD] Running legacy Make...")
    subprocess.run(['make', '-j', str(os.cpu_count())], cwd=config.root)

    print("[BUILD] Running modern CMake...")
    build_dir = os.path.join(config.root, 'build')
    if not os.path.exists(build_dir):
        os.makedirs(build_dir)
    
    cmake_cmd = ['cmake', '..']
    if asan:
        cmake_cmd.append('-DENABLE_ASAN=ON')
    if ubsan:
        cmake_cmd.append('-DENABLE_UBSAN=ON')
    for d in defines:
        cmake_cmd.append(f'-D{d}')
        
    subprocess.run(cmake_cmd, cwd=build_dir)
    subprocess.run(['make', '-j', str(os.cpu_count())], cwd=build_dir)

@cli.command()
@click.option('-v', '--verbose', is_flag=True, help='Verbose output')
@click.pass_obj
def test(config, verbose):
    """Run all tests (Python and C++)"""
    # 1. Run Python tests
    print("[TEST] Running Python tests...")
    env = config.get_env(standalone=True)
    env['PYTHONPATH'] = os.path.join(config.root, 'python') + (':' + env['PYTHONPATH'] if 'PYTHONPATH' in env else '')
    
    pytest_cmd = [sys.executable, '-m', 'pytest', 'tests/unit/python', 'tests/functional']
    if verbose:
        pytest_cmd.append('-v')
    subprocess.run(pytest_cmd, cwd=config.root, env=env)

    # 2. Run C++ tests via CTest
    print("\n[TEST] Running C++ tests...")
    build_dir = os.path.join(config.root, 'build')
    if os.path.exists(build_dir):
        ctest_cmd = ['ctest', '--output-on-failure']
        if verbose:
            ctest_cmd.append('-V')
        subprocess.run(ctest_cmd, cwd=build_dir)
    else:
        print("Warning: build directory not found. Run 'sniper-cli.py build' first.")

@cli.command(name='apx-test')
@click.pass_obj
def apx_test(config):
    """Run a full APX verification test: compile, record, and sim"""
    clang = config.get('clang_path')
    if not clang:
        print("Error: clang_path not found in config")
        return
    
    source = os.path.join(config.root, 'apx_test_inline.c')
    binary = os.path.join(config.root, 'apx_test_inline_bin')
    
    print(f"[APX_TEST] Compiling {source} with {clang}...")
    env = os.environ.copy()
    env['LD_LIBRARY_PATH'] = '/usr/lib/x86_64-linux-gnu:' + env.get('LD_LIBRARY_PATH', '')
    
    res = subprocess.run([clang, '-mapxf', source, '-o', binary], env=env)
    if res.returncode != 0:
        print("[APX_TEST] Compilation failed")
        return

    print("[APX_TEST] Starting simulation with -dmr...")
    ctx = click.get_current_context()
    ctx.invoke(sim, cmdline=[binary], sde_arch='-dmr', name='apx_test_auto')

if __name__ == '__main__':
    cli()
