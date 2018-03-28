#!/usr/bin/python

import yaml
import json
import sys

sys.stdout.write(json.dumps(yaml.load(sys.stdin), sort_keys=True, indent=4))

