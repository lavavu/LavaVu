#!/usr/bin/env python
cols = [[59,76,192],
[115,150,245],
[176,203,252],
[220,210,210],
[246,191,165],
[234,123,96],
[181,11,39]]
hexstr = ""
for c in cols:
    hexstr += "#"
    for o in c:
        hexstr += '{:02x}'.format(o)
    hexstr += " "
print hexstr
