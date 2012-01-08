/* This file is part of wmdump
 * Copyright (c) 2011 Gianni Tedesco
 * Released under the terms of GPLv3
*/
#ifndef _RTMP_PROTO_H
#define _RTMP_PROTO_H

#define RTMP_MSG_CHUNK_SZ		0x01
#define RTMP_MSG_CTL			0x04
#define RTMP_MSG_SERVER_BW		0x05
#define RTMP_MSG_CLIENT_BW		0x06
#define RTMP_MSG_AUDIO			0x08
#define RTMP_MSG_VIDEO			0x09
#define RTMP_MSG_FLEX_MESSAGE		0x11
#define RTMP_MSG_INFO			0x12
#define RTMP_MSG_INVOKE			0x14
#define RTMP_MAX_MSG			0x15

#define RTMP_HANDSHAKE_LEN		1536
#define RTMP_DEFAULT_CHUNK_SZ		128

#define RTMP_HDR_MAX_SZ			14

#define RTMP_MAX_CHANNELS		65600

#define RTMP_CTL_STREAM_BEGIN		0x0
#define RTMP_CTL_PING			0x6
#define RTMP_CTL_PONG			0x7

#endif /* _RTMP_PROTO_H */
