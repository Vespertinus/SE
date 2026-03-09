#!/usr/bin/python3

import yaml
import json
import sys

sys.stdout.write(json.dumps(yaml.load(sys.stdin, Loader=yaml.SafeLoader), sort_keys=True, indent=4))

