Import('AIC_ROOT')
Import('PRJ_KERNEL')
from building import *

cwd = GetCurrentDir()
CPPPATH = [cwd]

src = []
install = []

if GetDepend('PKG_AIC_GE_DEMOS'):
    src = Glob('*.c')
    src += Glob('effects/*.c')

if GetDepend('PKG_AIC_GE_DEMOS'):
    install = [('assets/', 'data/ge_demos/')] 

group = DefineGroup('ge-demos', src, depend = ['PKG_AIC_GE_DEMOS'], CPPPATH = CPPPATH, INSTALL=install)

Return('group')
