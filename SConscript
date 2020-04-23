from building import *

cwd = GetCurrentDir()
path  = [cwd + '/inc']
path += [cwd + '/inc/gsm']
src  = Glob('src/*.c')

src += Glob('sample/cmux_sample_gsm.c')

if GetDepend(['CMUX_USING_GSM']):
    src += Glob('src/gsm/*.c')

if GetDepend(['PKG_USING_PPP_DEVICE']):
	SrcRemove(src, "src/gsm/cmux_chat.c")

group = DefineGroup('cmux', src, depend = ['PKG_USING_CMUX'], CPPPATH = path)

Return('group')
