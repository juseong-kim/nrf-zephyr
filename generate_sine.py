import numpy as np
import math

n = 40
xs = np.linspace(0,1,n+1)
print(f"static float sine[{n}] = {{", end="")
for i, x in enumerate(xs):
    s = 50*math.sin(2*math.pi*x)+50
    # print(round(x,5),round(s,5))
    if i == len(xs)-1:
        pass
    else:
        print(round(s,5),end="")
        if i != len(xs)-2:
            print(", ", end="")
print("};")