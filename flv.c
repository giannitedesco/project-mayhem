/* This file is part of wmdump
 * Copyright (c) 2011 Gianni Tedesco
 * Released under the terms of GPLv3
*/
#include <wmdump.h>
#include <wmvars.h>
#include <mayhem.h>

#include <rtmp/amf.h>
#include <rtmp/rtmp.h>
#include <rtmp/proto.h>

#include <flv.h>

FILE *flv_creat(const char *fn)
{
	static const uint8_t hdr[] = {'F', 'L', 'V', 1, 5, 0, 0, 0, 9,
					0, 0, 0, 0};
	FILE *f;

	f = fopen(fn, "w");
	if ( NULL == f )
		return NULL;

	fwrite(hdr, sizeof(hdr), 1, f);

	return f;
}

struct flv_tag {
	uint8_t type;
	uint8_t len[3];
	uint8_t ts[3];
	uint8_t ts_high;
	uint8_t id[3];
}__attribute__((packed));

void flv_rip(FILE *f, struct rtmp_pkt *pkt, const uint8_t *buf, size_t sz)
{
	uint8_t prev_tag_sz[4];
	struct flv_tag tag;

	if ( NULL == f )
		return;
	if ( !sz )
		return;

	switch(pkt->type) {
	case RTMP_MSG_AUDIO:
		if ( sz <= 2 )
			return;
		break;
	case RTMP_MSG_VIDEO:
		if ( sz <= 6 )
			return;
		break;
	default:
		break;
	}

	memset(&tag, 0, sizeof(tag));
	tag.type = pkt->type;
	tag.len[0] = (sz >> 16) & 0xff;
	tag.len[1] = (sz >> 8) & 0xff;
	tag.len[2] = (sz) & 0xff;
	tag.ts[0] = (pkt->ts >> 16) & 0xff;
	tag.ts[1] = (pkt->ts >> 8) & 0xff;
	tag.ts[2] = (pkt->ts) & 0xff;
	tag.ts_high = (pkt->ts >> 24) & 0xff;

	fwrite(&tag, sizeof(tag), 1, f);
	fwrite(buf, sz, 1, f);

	sz += sizeof(tag);
	prev_tag_sz[0] = (sz >> 24) & 0xff;
	prev_tag_sz[1] = (sz >> 16) & 0xff;
	prev_tag_sz[2] = (sz >> 8) & 0xff;
	prev_tag_sz[3] = (sz) & 0xff;
	fwrite(prev_tag_sz, sizeof(prev_tag_sz), 1, f);
}

void flv_close(FILE *f)
{
	if ( f )
		fclose(f);
}
