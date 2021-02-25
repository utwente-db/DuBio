import matplotlib.pyplot as plt
import numpy as np

sizes = [1, 5, 10, 50, 100, 500,1000, 5000, 10000]
times = [4.303966999999798, 45.23709600000014, 190.63361200000006, 372.8139380000002, 726.543607, 1437.9142520000005, 2212.2525219999998, 2846.2377389999992, 4260.695452]

m = [ #   0         1         2         3         4         5         6
  [    7.07,    11.17,     5.39,     2.34,     2.06,     9.78,     8.84],
  [   14.02,    24.19,    12.19,     6.37,     6.62,    31.88,    28.42],
  [   22.63,    36.25,    20.90,    10.95,    11.30,    58.53,    51.74],
  [   82.00,   259.38,    86.71,    37.33,    41.04,   270.94,   238.99],
  [  202.36,   588.61,   168.99,    70.45,    76.90,   538.52,   473.87],
  [ 1162.53,  2758.34,   827.01,   336.00,   364.34,  2706.79,  2369.57],
  [ 2279.31,  5097.94,  1642.93,   693.74,   714.43,  5514.74,  4784.20],
  [12896.26, 29311.17, 11675.97,  3848.06,  3648.58, 27159.66, 23874.56],
  [25226.79, 54471.42, 16996.36,  6835.25,  7323.33, 54150.26, 60146.01]
]


X = sizes
Y = times

plt.title('BDD experiments on random BDD strings 1K - 10M')

plt.xlabel('# BDD Strings x 1000')
plt.ylabel('Execution time in ms.')

def doplot(x,m,m_col,color, style):
    Y = []
    for tr in m:
        Y.append(tr[m_col])
    plt.plot(x,Y, color=color, linestyle=style, linewidth=3)

doplot(X,m,0,'red','dashed')
doplot(X,m,1,'red','solid')
#
doplot(X,m,2,'green','solid')
doplot(X,m,3,'green','dotted')
#
doplot(X,m,4,'blue','dashed')
doplot(X,m,5,'blue','solid')
#
doplot(X,m,6,'blue','dotted')
plt.show()
