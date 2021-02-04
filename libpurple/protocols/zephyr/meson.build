ZEPHYR_INTERNAL_SOURCES = [
	'ZAsyncLocate.c',
	'ZCkAuth.c',
	'ZCkIfNot.c',
	'ZClosePort.c',
	'ZCmpUID.c',
	'ZCmpUIDP.c',
	'ZFlsLocs.c',
	'ZFlsSubs.c',
	'ZFmtAuth.c',
	'ZFmtList.c',
	'ZFmtNotice.c',
	'ZFmtRaw.c',
	'ZFmtRawLst.c',
	'ZFmtSmRaw.c',
	'ZFreeNot.c',
	'ZGetLocs.c',
	'ZGetSender.c',
	'ZGetSubs.c',
	'ZGetWGPort.c',
	'ZIfNotice.c',
	'ZInit.c',
	'ZLocations.c',
	'ZMakeAscii.c',
	'ZMkAuth.c',
	'ZNewLocU.c',
	'ZOpenPort.c',
	'ZParseNot.c',
	'ZPeekNot.c',
	'ZPeekPkt.c',
	'ZPending.c',
	'ZReadAscii.c',
	'ZRecvNot.c',
	'ZRecvPkt.c',
	'ZRetSubs.c',
	'ZSendList.c',
	'ZSendNot.c',
	'ZSendPkt.c',
	'ZSetDest.c',
	'ZSubs.c',
	'ZVariables.c',
	'ZWait4Not.c',
	'ZhmStat.c',
	'Zinternal.c',
	'mit-copyright.h',
	'mit-sipb-copyright.h',
	'zephyr_err.h',
	'zephyr_internal.h'
]

ZEPHYR_SOURCES = [
	'internal.h',
	'sysdep.h',
	'zephyr.c',
	'zephyr.h',
	'zephyr_account.h',
	'zephyr_html.c',
	'zephyr_html.h',
	'zephyr_tzc.c',
	'zephyr_tzc.h',
	'zephyr_zeph02.c',
	'zephyr_zeph02.h',
]

extdep = krb4
if EXTERNAL_LIBZEPHYR
	extdep = ext_zephyr
else
	ZEPHYR_SOURCES += ZEPHYR_INTERNAL_SOURCES
endif

if DYNAMIC_ZEPHYR
	zephyr_prpl = shared_library('zephyr', ZEPHYR_SOURCES,
	    c_args : '-Dlint',
	    dependencies : [extdep, libpurple_dep, glib],
	    install : true, install_dir : PURPLE_PLUGINDIR)
endif
