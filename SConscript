from building import *

cwd = GetCurrentDir()
path = [cwd + '/inc']
src  = Glob('src/*.c')
src += Glob('sample/sample.c')

src += Glob('class/cmux_air720.c')

if GetDepend(['PKG_USING_PPP_DEVICE']):
	SrcRemove(src, "src/cmux_chat.c")

group = DefineGroup('cmux', src, depend = ['PKG_USING_CMUX'], CPPPATH = path)

Return('group')
