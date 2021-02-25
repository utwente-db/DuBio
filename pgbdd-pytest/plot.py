import matplotlib.pyplot as plt
import numpy as np

sizes = [1, 10, 40, 80, 160, 320, 480, 640, 960]
times = [4.303966999999798, 45.23709600000014, 190.63361200000006, 372.8139380000002, 726.543607, 1437.9142520000005, 2212.2525219999998, 2846.2377389999992, 4260.695452]


Y = times
X = sizes

plt.title('select max(prob(dict,bdd)) from bdd_raw_1K, Dict WHERE Dict.name=\'dict_*K;\'')
plt.xlabel('Size of dictionary in Kb')
plt.ylabel('Execution time in ms.')

plt.plot(X,Y,color='red',linewidth=2)
plt.show()
