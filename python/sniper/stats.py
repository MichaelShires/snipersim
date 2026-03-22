# python/sniper/stats.py
import os
import sys
import shutil

class SniperStats:
    def __init__(self, config):
        self.config = config
        tools_path = os.path.join(self.config.root, 'tools')
        if tools_path not in sys.path:
            sys.path.append(tools_path)

    def get_stats(self, results_dir, jobid=0, silent=False):
        import sniper_stats
        import sniper_lib
        try:
            cfg_file = os.path.join(results_dir, 'sim.cfg')
            if not os.path.exists(cfg_file):
                root_cfg = os.path.join(self.config.root, 'sim.cfg')
                if os.path.exists(root_cfg):
                    shutil.copy(root_cfg, cfg_file)
            
            stats_obj = sniper_stats.SniperStats(resultsdir=results_dir, jobid=jobid)
            try:
                data = stats_obj.get_results(partial=['roi-begin', 'roi-end'])
            except:
                data = stats_obj.get_results(partial=None)
            return data
        except Exception as e:
            if not silent:
                print(f"Error reading stats in {results_dir}: {e}")
            return None

    def dump(self, results_dir, filter_str=None):
        data = self.get_stats(results_dir)
        if not data:
            return
        
        results = data.get('results', {})
        for key in sorted(results.keys()):
            if not filter_str or filter_str in key:
                print(f"{key:50} | {results[key]}")

    def compare_stats(self, stats1, stats2, threshold=0.01):
        """Compare two stat dictionaries and return differences above threshold (%)"""
        diffs = {}
        all_keys = set(stats1.keys()) | set(stats2.keys())
        
        for key in all_keys:
            val1 = stats1.get(key, 0)
            val2 = stats2.get(key, 0)
            
            if isinstance(val1, list) and isinstance(val2, list):
                for i, (v1, v2) in enumerate(zip(val1, val2)):
                    subkey = f"{key}[{i}]"
                    if isinstance(v1, (int, float)) and isinstance(v2, (int, float)):
                        if v1 == 0:
                            if v2 != 0:
                                diffs[subkey] = (v1, v2, float('inf'))
                        else:
                            diff = abs(v1 - v2) / abs(v1)
                            if diff > threshold:
                                diffs[subkey] = (v1, v2, diff)
            elif isinstance(val1, (int, float)) and isinstance(val2, (int, float)):
                if val1 == 0:
                    if v2 != 0:
                        diffs[key] = (val1, val2, float('inf'))
                else:
                    diff = abs(val1 - val2) / abs(val1)
                    if diff > threshold:
                        diffs[key] = (val1, val2, diff)
            elif val1 != val2:
                diffs[key] = (val1, val2, None)
                
        return diffs
