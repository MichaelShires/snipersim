# python/sniper/report_manager.py
import os
import fnmatch
import json
import csv
import sys
from .stats import SniperStats
from .run_manager import RunManager

try:
    from rich.console import Console
    from rich.table import Table
    console = Console()
except ImportError:
    console = None

class ReportManager:
    def __init__(self, config):
        self.config = config
        self.run_manager = RunManager(config)
        self.stats_tool = SniperStats(config)

    def generate_report(self, pattern, metrics, format='table'):
        runs = self.run_manager.list_runs()
        matched_runs = [r for r in runs if fnmatch.fnmatch(r['name'], pattern)]
        
        if not matched_runs:
            print(f"No runs found matching pattern: {pattern}")
            return

        if not metrics:
            metrics = ['core.instructions', 'core.cycles', 'core.cpi']

        data = []
        for run in matched_runs:
            # Use silent=True to avoid traceback noise
            run_stats = self.stats_tool.get_stats(run['path'], silent=True)

            if not run_stats:
                continue
            
            row = {'run': run['name']}
            params = run.get('metadata', {}).get('params', {})
            for p_key, p_val in params.items():
                row[f"param:{p_key}"] = p_val
                
            results = run_stats.get('results', {})
            for m in metrics:
                if m in results:
                    row[m] = results[m]
                else:
                    matches = [k for k in results.keys() if m in k]
                    if matches:
                        row[m] = results[matches[0]]
                    else:
                        row[m] = 'N/A'
            data.append(row)

        if not data:
            print("No valid stats found for matched runs.")
            return

        if format == 'csv':
            self._print_csv(data)
        elif format == 'json':
            print(json.dumps(data, indent=2))
        else:
            self._print_table(data, metrics)

    def _print_table(self, data, requested_metrics):
        if not console:
            header = list(data[0].keys())
            print(" | ".join(f"{h:20}" for h in header))
            print("-" * (22 * len(header)))
            for row in data:
                print(" | ".join(f"{str(row.get(h, '')):20}" for h in header))
            return

        table = Table(title="Sniper Simulation Aggregate Report")
        header = list(data[0].keys())
        for h in header:
            table.add_column(h, style="cyan" if "param" in h else "magenta")
        
        for row in data:
            table.add_row(*[str(row.get(h, '')) for h in header])
            
        console.print(table)

    def _print_csv(self, data):
        if not data: return
        writer = csv.DictWriter(sys.stdout, fieldnames=data[0].keys())
        writer.writeheader()
        writer.writerows(data)
