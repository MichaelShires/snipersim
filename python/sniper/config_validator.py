import os
import re

class ConfigValidator:
    def __init__(self, sniper_root):
        self.root = sniper_root
        self.valid_keys = set()
        self.valid_sections = set()
        self._load_schema()

    def _load_schema(self):
        config_dir = os.path.join(self.root, 'config')
        if not os.path.isdir(config_dir):
            return

        for filename in os.listdir(config_dir):
            if filename.endswith('.cfg'):
                self._parse_cfg(os.path.join(config_dir, filename))

    def _parse_cfg(self, path):
        current_section = None
        try:
            with open(path, 'r', errors='replace') as f:
                for line in f:
                    line = line.strip()
                    if not line or line.startswith('#') or line.startswith('//'):
                        continue
                    
                    section_match = re.match(r'^\[([a-zA-Z0-9_/]+)\]', line)
                    if section_match:
                        current_section = section_match.group(1)
                        self.valid_sections.add(current_section)
                        continue

                    if current_section and '=' in line:
                        key = line.split('=')[0].strip()
                        if key:
                            self.valid_keys.add(f"{current_section}/{key}")
        except Exception as e:
            pass # Silent during schema load

    def validate_key(self, full_key):
        normalized_key = re.sub(r'\[\d+\]', '', full_key)
        
        if normalized_key in self.valid_keys:
            return True
        
        # Stricter validation: if it's not in valid_keys, it's a potential typo
        return False

    def validate_options(self, options):
        issues = []
        for opt in options:
            clean_opt = opt
            if clean_opt.startswith('--'):
                clean_opt = clean_opt[2:]
            
            if '=' not in clean_opt:
                issues.append(('ERROR', f"Invalid option format (missing '='): {opt}"))
                continue
                
            key, value = clean_opt.split('=', 1)
            if not self.validate_key(key):
                suggestions = self.get_suggestions(key)
                msg = f"Unknown configuration key: {key}"
                if suggestions:
                    msg += f". Did you mean: {', '.join(suggestions)}?"
                issues.append(('WARNING', msg))
                
        return issues

    def get_suggestions(self, key, limit=3):
        import difflib
        return difflib.get_close_matches(key, list(self.valid_keys), n=limit, cutoff=0.6)
