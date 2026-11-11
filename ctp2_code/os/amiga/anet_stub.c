/*
 * The ActiveNet calls the game makes (net/io, ui/netshell), for the Amiga
 * builds, which have no multiplayer: every call fails as an unreachable
 * network would (dp_RES_BAD), so the multiplayer screens report that no
 * transport is there. Generated from the prototypes in libs/anet/h/anet.h.
 */
#include "anet.h"

DP_API dp_result_t DP_APIX dpClose(dp_t *dp)
{
	return dp_RES_BAD;
}

DP_API dp_result_t DP_APIX dpCreate(dp_t **pdp, dp_transport_t *transport, commInitReq_t *params, const char *thawFilename)
{
	return dp_RES_BAD;
}

DP_API dp_result_t DP_APIX dpCreatePlayer(dp_t *dp, dpEnumPlayersCallback_t cb, void *context, dp_char_t *name)
{
	return dp_RES_BAD;
}

DP_API dp_result_t DP_APIX dpDestroy(dp_t *dp, sint32 flags)
{
	return dp_RES_BAD;
}

DP_API dp_result_t DP_APIX dpDestroyPlayer(dp_t *dp, dpid_t id)
{
	return dp_RES_BAD;
}

DP_API dp_result_t DP_APIX dpDownloadUpdate(dp_t *dp, const dp_appParam_t *app)
{
	return dp_RES_BAD;
}

DP_API dp_result_t DP_APIX dpEnableNewPlayers(dp_t *dp, sint32 enable)
{
	return dp_RES_BAD;
}

DP_API dp_result_t DP_APIX dpEnumPlayers(dp_t *dp, dp_session_t *s, dpEnumPlayersCallback_t cb, void *context, sint32 timeout)
{
	return dp_RES_BAD;
}

DP_API dp_result_t DP_APIX dpEnumPorts(const dp_transport_t *transport, commPortName_t *ports, sint32 maxports, sint32 *pnPorts)
{
	return dp_RES_BAD;
}

DP_API dp_result_t DP_APIX dpEnumSessions(dp_t *dp, dp_session_t *sDesc, char *mhn, sint32 timeout, dpEnumSessionsCallback_t cb, void *context)
{
	return dp_RES_BAD;
}

DP_API dp_result_t DP_APIX dpEnumTransports(dp_transport_t *path, dpEnumTransportCallback_t cb, void *context)
{
	return dp_RES_BAD;
}

DP_API dp_result_t DP_APIX dpGetAppVersion(dp_t* dp, dp_appParam_t *app)
{
	return dp_RES_BAD;
}

DP_API dp_result_t DP_APIX dpGetCaps(dp_t *dp, dp_caps_t *info, sint32 flags)
{
	return dp_RES_BAD;
}

DP_API dp_result_t DP_APIX dpGetCurrentTransportInfo(dp_t *dp, dp_transport_t *path, comm_driverInfo_t *info)
{
	return dp_RES_BAD;
}

DP_API dp_result_t DP_APIX dpGetPlayerData(dp_t *dp, dpid_t idPlayer, sint32 key, void *buf, size_t *pbuflen, sint32 flags)
{
	return dp_RES_BAD;
}

DP_API dp_result_t DP_APIX dpGetPlayerName(dp_t *dp, dpid_t id, dp_char_t *buf, size_t bufLen)
{
	return dp_RES_BAD;
}

DP_API dp_result_t DP_APIX dpGetSessionDesc(dp_t *dp, dp_session_t *buf, size_t *pbuflen)
{
	return dp_RES_BAD;
}

DP_API dp_result_t DP_APIX dpGetSessionId(dp_t *dp, const dp_session_t *sess, char *id, sint32 *pidlen)
{
	return dp_RES_BAD;
}

DP_API dp_result_t DP_APIX dpOpen(dp_t *dp, dp_session_t *s, dpEnumSessionsCallback_t cb, void *context)
{
	return dp_RES_BAD;
}

DP_API dp_result_t DP_APIX dpReadyToFreeze(dp_t *dp)
{
	return dp_RES_BAD;
}

DP_API dp_result_t DP_APIX dpReceive(dp_t *dp, dpid_t *idFrom, dpid_t *idTo, sint32 flags, void *buffer, size_t *size)
{
	return dp_RES_BAD;
}

DP_API dp_result_t DP_APIX dpRequestObjectDeltas(dp_t *dp, sint32 monitor, const char *key, sint32 keylen)
{
	return dp_RES_BAD;
}

DP_API dp_result_t DP_APIX dpSend(dp_t *dp, dpid_t idFrom, dpid_t idTo, sint32 flags, void *buffer, size_t size)
{
	return dp_RES_BAD;
}

DP_API dp_result_t DP_APIX dpSetActiveThread(dp_t *dp)
{
	return dp_RES_BAD;
}

DP_API dp_result_t DP_APIX dpSetGameServer(dp_t *dp, char *masterHostName)
{
	return dp_RES_BAD;
}

DP_API dp_result_t DP_APIX dpSetGameServerEx(dp_t *dp, char *masterHostName, dp_species_t sessionType)
{
	return dp_RES_BAD;
}

DP_API dp_result_t DP_APIX dpSetPingIntervals(dp_t *dp, sint32 piggybackPingIntervalMS, sint32 forcedPingIntervalMS)
{
	return dp_RES_BAD;
}

DP_API dp_result_t DP_APIX dpSetPlayerBlob(dp_t *dp, dpid_t id, void *blob, size_t bloblen)
{
	return dp_RES_BAD;
}

DP_API dp_result_t DP_APIX dpSetPlayerData(dp_t *dp, dpid_t idPlayer, sint32 key, void *buf, size_t buflen, sint32 flags)
{
	return dp_RES_BAD;
}

DP_API dp_result_t DP_APIX dpSetPlayerName(dp_t *dp, dpid_t id, dp_char_t *name)
{
	return dp_RES_BAD;
}

DP_API dp_result_t DP_APIX dpSetSessionDesc(dp_t *dp, const dp_session_t *buf, sint32 flags)
{
	return dp_RES_BAD;
}

DP_API dp_result_t DP_APIX dpAccountLoginA(dp_t *dp, const char *username, const char *password)
{
	return dp_RES_BAD;
}

DP_API dp_result_t dpFlush(dp_t *dp)
{
	return dp_RES_BAD;
}

DP_API void DP_APIX dpini_SetFile(const char *iniFileName)
{
}
