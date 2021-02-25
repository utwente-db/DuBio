import random

SEED             = 1963

MAX_LEVELS       = 1
MAX_CLUSTER      = 4
MAX_CLUSTER_SIZE = 3
NOT_MODULO       = 5
N_VARS           = 3
N_VALS           = 4

class BddGenerator:
  def __init__(self, seed=SEED, max_levels=MAX_LEVELS, max_cluster=MAX_CLUSTER, max_cluster_size=MAX_CLUSTER_SIZE, not_modulo=NOT_MODULO, n_vars=N_VARS, n_vals=N_VALS):
    self.MAX_LEVELS       = max_levels
    self.MAX_CLUSTER      = max_cluster
    self.MAX_CLUSTER_SIZE = max_cluster_size
    self.NOT_MODULO       = not_modulo
    self.N_VARS           = n_vars
    self.N_VALS           = n_vals
    #
    random.seed(SEED)

  def base_dict(self):
    res = ''
    for i in range(0,self.N_VARS+1):
       v = self.genvar(i)
       for j in range(0,self.N_VALS+1):
         res = res + v+'='+str(j)+':'+ "{:.2f}".format(random.uniform(0,1.0)) +';'
    return res

  def extra_dict(self,extra):
    res = ''
    for i in range(0,self.N_VARS+1):
       v = self.genvar(i)
       for j in range(0,self.N_VALS+1):
         res = res + v+'='+str(j)+':'+ "{:.2f}".format(random.uniform(0,1.0)) +';'
    #
    for i in range(self.N_VARS+2,self.N_VARS+2+extra):
       v = self.genvar(i)
       for j in range(0,self.N_VALS+1):
         res = res + v+'='+str(j)+':'+ "{:.2f}".format(random.uniform(0,1.0)) +';'
    #
    return res

  def randint(self,f,t):
    return random.randint(f,t)
  def randint(self,f,t):
    return random.randint(f,t)

  def genvar(self,n):
    res = ""
    while n >= 0:
      res = 'abcdefghijklmnopqrstuvwxyz'[n%26] + res
      n = n//26-1
    return res

  def rand_var(self):
    return self.genvar(self.randint(0,self.N_VARS))

  def rand_val(self):
    return str(self.randint(0,self.N_VALS))

  def rand_and_or(self):
    return '&' if (self.randint(0,1)==0) else '|'

  def rand_not(self):
    return '!' if (self.randint(0,self.NOT_MODULO)==0) else ''
  
  def generate_expression(self,level):
    if self.randint(0,self.MAX_LEVELS-1) > level:
      return self.generate_level(level+1)
    else: 
      return self.rand_not()+self.rand_var() + '=' + self.rand_val()

  def generate_cluster(self,level):
    n_expr = self.randint(1,self.MAX_CLUSTER_SIZE)
    op = self.rand_and_or()
    res = '('
    for i in range(0,n_expr):
      if i > 0:
        res = res + op
      res = res + self.generate_expression(level)
    return res + ')'

  def generate_level(self,level):
    n_clusters = self.randint(1,self.MAX_CLUSTER)
    op = self.rand_and_or()
    res = '('
    for i in range(0,n_clusters):
      if i > 0:
        res = res + op
      res = res + self.generate_expression(level)
    return res + ')'

  def expression(self):
    return self.generate_level(0)
  

if __name__ == "__main__":
  brg = BddGenerator()
  #for i in range(0,30):
  #  print(brg.expression())
  print(brg.base_dict())
