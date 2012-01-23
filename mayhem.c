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
#include <rtmp/netstatus.h>
#include <flv.h>

#include "cvars.h"
#include "mayhem-amf.h"

#include <time.h>

#define MAYHEM_STATE_ABORT		0
#define MAYHEM_STATE_CONNECTING		1
#define MAYHEM_STATE_FROZEN		2
#define MAYHEM_STATE_AUTHORIZED		3
#define MAYHEM_STATE_GOT_STREAM		4
#define MAYHEM_STATE_PLAYING		5
#define MAYHEM_STATE_PAUSED		6

struct _mayhem {
	char *bitch;
	rtmp_t rtmp;
	netstatus_t ns;
	FILE *flv;
	unsigned int state;
	unsigned int sid;
};

struct naiad_room {
	unsigned int flags;
	const char *topic;
};

static void mayhem_abort(struct _mayhem *m)
{
	m->state = MAYHEM_STATE_ABORT;
}

static int update_bitch(struct _mayhem *m, const char *bitch)
{
	char *b = strdup(bitch);

	if ( NULL == b )
		return 0;

	m->bitch = b;
	return 1;
}

static int NaiadAuthorize(mayhem_t m, int code,
				const char *nick,
				const char *bitch,
				unsigned int sid,
				struct naiad_room *room)
{
	printf("NaiadAuthorize: code = %u\n", code);
	printf(" your nickname: %s\n", nick);
	printf(" performer: %s\n", bitch);
	printf(" room flags: %d (0x%x)\n", room->flags, room->flags);
	printf(" topic is: %s\n", (room->topic) ? room->topic : "");
	update_bitch(m, bitch);
	m->state = MAYHEM_STATE_AUTHORIZED;
	return 1;
}

static int NaiadFreeze(mayhem_t m, int code, void *u1, int u2, const char *desc)
{
	printf("NaiadFreeze: %d: %s\n", code, desc);
	m->state = MAYHEM_STATE_FROZEN;
	//m->state = MAYHEM_STATE_AUTHORIZED;
	return 1;
}

static int i_auth(mayhem_t m, invoke_t inv)
{
	amf_t o_rc, o_nick, o_bitch, o_sid, o_room;
	amf_t o_topic, o_flags;
	struct naiad_room room;

	if ( amf_invoke_nargs(inv) < 26 ) {
		printf("mayhem: too few args in NaiadAuthorize\n");
		return 0;
	}

	o_rc = amf_invoke_get(inv, 1);
	o_nick = amf_invoke_get(inv, 6);
	o_bitch = amf_invoke_get(inv, 7);
	o_sid = amf_invoke_get(inv, 12);
	o_room = amf_invoke_get(inv, 25);

	if ( amf_type(o_rc) != AMF_NUMBER ||
			amf_type(o_nick) != AMF_STRING ||
			amf_type(o_bitch) != AMF_STRING ||
			amf_type(o_sid) != AMF_STRING ||
			amf_type(o_room) != AMF_OBJECT ) {
		printf("mayhem: type mismatch in NaiadAuthorize args\n");
		return 0;
	}
	
	o_flags = amf_object_get(o_room, "flags");
	o_topic = amf_object_get(o_room, "roomtopic");
	if ( NULL == o_flags ||
			NULL == o_topic ||
			amf_type(o_flags) != AMF_NUMBER ) {
		printf("mayhem: Attribs not found in NaiadAuthorize room\n");
		return 0;
	}

	room.flags = amf_get_number(o_flags);
	switch(amf_type(o_topic)) {
	case AMF_STRING:
		room.topic = amf_get_string(o_topic);
		break;
	case AMF_NULL:
		room.topic = NULL;
		break;
	default:
		printf("mayhem: bad type for roomtopic\n");
		return 0;
	}

	return NaiadAuthorize(m,
				amf_get_number(o_rc),
				amf_get_string(o_nick),
				amf_get_string(o_bitch),
				atoi(amf_get_string(o_sid)),
				&room);
}


static int i_freeze(mayhem_t m, invoke_t inv)
{
	amf_t o_rc, o_desc;
	if ( amf_invoke_nargs(inv) < 5 ) {
		printf("mayhem: too few args in NaiadFreeze\n");
		return 0;
	}

	o_rc = amf_invoke_get(inv, 1);
	o_desc = amf_invoke_get(inv, 4);

	if ( amf_type(o_rc) != AMF_NUMBER || amf_type(o_desc) != AMF_STRING ) {
		printf("mayhem: type mismatch in NaiadFreeze args\n");
		return 0;
	}

	return NaiadFreeze(m, amf_get_number(o_rc),
				NULL, -1, amf_get_string(o_desc));
}

struct user {
	unsigned int flags;
	const char *id;
	const char *name;
};

static int user_parse(amf_t obj, struct user *usr)
{
	amf_t o_flags, o_id, o_name;

	if ( NULL == obj || amf_type(obj) != AMF_OBJECT )
		return 0;

	memset(usr, 0, sizeof(*usr));

	o_flags = amf_object_get(obj, "flags");
	o_id = amf_object_get(obj, "id");
	o_name = amf_object_get(obj, "userName");

	if ( NULL == o_id || NULL == o_name ) {
		printf("mayhem: missing element of user object\n");
		return 0;
	}

	if ( (o_flags && amf_type(o_flags) != AMF_NUMBER) ||
		amf_type(o_id) != AMF_STRING ) {
		printf("mayhem: type mismatch in user object\n");
		return 0;
	}

	if ( o_flags )
		usr->flags = amf_get_number(o_flags);
	usr->id = amf_get_string(o_id);

	/* can be null or undefined */
	if ( amf_type(o_name) == AMF_STRING )
		usr->name = amf_get_string(o_name);

	return 1;
}

/* These are weird, we seem to get empty ones, don't know what 'ac' means,
 * must be incrementally updated somehow.
*/
static int i_userlist(mayhem_t m, invoke_t inv)
{
	unsigned int i, nargs;
	static int done;
	amf_t obj;

	nargs = amf_invoke_nargs(inv);
	if ( nargs < 5 ) {
		printf("mayhem: too few args in NaiadUserList\n");
		return 0;
	}

	if ( !done )
		return 1;

	printf("mayhem: user list\n");

	obj = amf_invoke_get(inv, nargs - 1);
	if ( obj && amf_type(obj) == AMF_OBJECT ) {
		amf_t ac;
		ac = amf_object_get(obj, "ac");
		if ( ac && amf_type(ac) == AMF_NUMBER ) {
			printf(" ac = %f\n", amf_get_number(ac));
		}
	}

	for(i = 0; i < nargs - 5; i++) {
		struct user usr;
		amf_t obj;

		obj = amf_invoke_get(inv, 3 + i);
		if ( !user_parse(obj, &usr) )
			continue;

		printf(" - %s ('%s')\n", usr.name, usr.id);
	}

	done = 1;
	return 1;
}

/* NaiadAddChat(number, nll, number, string, string chat, bool, bool, bool,
 *		string, object, { .flags = number, .goldamtstr = number }
 */
static int i_chat(mayhem_t m, invoke_t inv)
{
	amf_t user, chat;

	if ( amf_invoke_nargs(inv) < 6 ) {
		printf("mayhem: too few args in NaiadAddChat\n");
		return 0;
	}

	user = amf_invoke_get(inv, 4);
	chat = amf_invoke_get(inv, 5);
	if ( NULL == user || NULL == chat )
		return 0;
	if ( amf_type(user) != AMF_STRING || amf_type(chat) != AMF_STRING )
		return 0;

	printf("mayhem: chat: <%s> %s\n",
		amf_get_string(user),
		amf_get_string(chat));
	return 1;
}

/* NaiadPledgeGold(number, null, {.amount = number, .status = number} */
static int i_gold(mayhem_t m, invoke_t inv)
{
	amf_t obj, amt, status;
	unsigned int a = 0, s = 0;

	if ( amf_invoke_nargs(inv) < 4 ) {
		printf("mayhem: too few args in NaiadPledgeGold\n");
		return 0;
	}

	obj = amf_invoke_get(inv, 3);
	if ( NULL == obj || amf_type(obj) != AMF_OBJECT ) {
		printf("mayhem: bad argument\n");
		return 0;
	}

	amt = amf_object_get(obj, "amount");
	status = amf_object_get(obj, "status");
	if ( amt && amf_type(amt) == AMF_NUMBER )
		a = amf_get_number(amt);
	if ( status && amf_type(status) == AMF_NUMBER )
		s = amf_get_number(status);

	printf("mayhem: gold pledged: %d (flags = %d)\n", a / 100, s);
	return 1;
}

static int naiad_dispatch(mayhem_t m, invoke_t inv, const char *method)
{
	static const struct {
		const char *method;
		int (*call)(mayhem_t m, invoke_t inv);
	}tbl[] = {
		{.method = "NaiadFreeze", .call = i_freeze},
		{.method = "NaiadAuthorized", .call = i_auth},
		{.method = "NaiadUserList", .call = i_userlist},
		{.method = "NaiadAddChat", .call = i_chat},
		{.method = "NaiadPledgeGold", .call = i_gold},
		/* NaiadPreGoldShow */
		/* NaiadGoldShow */
	};
	unsigned int i;

	for(i = 0; i < ARRAY_SIZE(tbl); i++) {
		if ( strcmp(tbl[i].method, method) )
			continue;
		if ( !(*tbl[i].call)(m, inv) )
			return -1;
		return 1;
	}
	return 0;
}

static int dispatch(void *priv, invoke_t inv)
{
	struct _mayhem *m = priv;
	amf_t method;
	int ret;

	if ( amf_invoke_nargs(inv) < 1 ) {
		printf("mayhem: missing method name in invoke\n");
		return 0;
	}

	method = amf_invoke_get(inv, 0);
	if ( NULL == method || amf_type(method) != AMF_STRING ) {
		printf("mayhem: bad method name in invoke\n");
		return 0;
	}

	/* try dispatch the method call */
	ret = naiad_dispatch(m, inv, amf_get_string(method));
	if ( ret < 0 )
		return 0;
	if ( ret )
		return 1;

	/* ret == 0, means unhandled, lets try netstatus */
	ret = netstatus_invoke(m->ns, inv);
	if ( ret < 0 )
		return 0;
	if ( !ret )
		return 0; /* unhandled */

	/* TODO: We may need to handle netstatus status and reflect it
	 * in overall application state
	*/
	switch(netstatus_state(m->ns)) {
	case NETSTATUS_STATE_CREATED:
		m->state = MAYHEM_STATE_GOT_STREAM;
		break;
	case NETSTATUS_STATE_PLAYING:
		printf("mayhem: playing...\n");
		m->state = MAYHEM_STATE_PLAYING;
		break;
	default:
		break;
	}

	return 1;
}

static int invoke_connect(struct _mayhem *m, struct _wmvars *v)
{
	invoke_t inv;
	int ret = 0;
	inv = mayhem_amf_connect(v);
	if ( NULL == inv )
		goto out;
	ret = rtmp_invoke(m->rtmp, 3, 0, inv);
	if ( ret ) {
		m->state = MAYHEM_STATE_CONNECTING;
		netstatus_set_state(m->ns, NETSTATUS_STATE_CONNECT_SENT);
	}
	amf_invoke_free(inv);
out:
	return ret;
}

static int invoke_start(struct _mayhem *m)
{
	invoke_t inv;
	int ret = 0;
	inv = mayhem_amf_start();
	if ( NULL == inv )
		goto out;
	ret = rtmp_flex_invoke(m->rtmp, 3, 0, inv);
	if ( ret ) {
		m->state = MAYHEM_STATE_CONNECTING;
		netstatus_set_state(m->ns, NETSTATUS_STATE_CONNECT_SENT);
	}
	amf_invoke_free(inv);
out:
	return ret;
}

static int create_stream(struct _mayhem *m, double num)
{
	if (netstatus_state(m->ns) == NETSTATUS_STATE_CONNECTED) {
		return netstatus_createstream(m->ns, 2.0);
	}
	return 1;
}

static int play(struct _mayhem *m)
{
	if (netstatus_state(m->ns) == NETSTATUS_STATE_CREATED ) {
		return netstatus_play(m->ns, amf_stringf("%d", m->sid));
	}
	return 1;
}

void mayhem_close(mayhem_t m)
{
	if ( m ) {
		flv_close(m->flv);
		free(m->bitch);
		netstatus_free(m->ns);
		rtmp_close(m->rtmp);
		free(m);
	}
}

static int notify(void *priv, struct rtmp_pkt *pkt,
			const uint8_t *buf, size_t sz)
{
	struct _mayhem *m = priv;
	invoke_t inv;
	amf_t a;

	inv = amf_invoke_from_buf(buf, sz);
	if ( NULL == inv )
		return 1;

	if ( amf_invoke_nargs(inv) < 1 )
		return 1;

	a = amf_invoke_get(inv, 0);
	if ( NULL == a || amf_type(a) != AMF_STRING )
		return 1;

	if ( strcmp(amf_get_string(a), "onMetaData") )
		return 1;

	printf("Video metadata:\n");
	amf_invoke_pretty_print(inv);
	amf_invoke_free(inv);

	invoke_start(m);

	flv_rip(m->flv, pkt, buf, sz);

	return 1;
}

static int rip(void *priv, struct rtmp_pkt *pkt,
			const uint8_t *buf, size_t sz)
{
	struct _mayhem *m = priv;
	flv_rip(m->flv, pkt, buf, sz);
	return 1;
}

static int stream_start(void *priv)
{
	struct _mayhem *m = priv;
	char buf[((m->bitch) ? strlen(m->bitch) : 64) + 128];
	char tmbuf[128];
	struct tm *tm;
	time_t t;

	t = time(NULL);

	tm = localtime(&t);
	strftime(tmbuf, sizeof(tmbuf), "%F-%H-%M-%S", tm);
	snprintf(buf, sizeof(buf), "%s-%s.flv",
		(m->bitch) ? m->bitch : "UNKNOWN", tmbuf);

	if ( m->flv )
		flv_close(m->flv);
	m->flv = flv_creat(buf);

	printf("mayhem: create FLV: %s\n", buf);
	return 1;
}

/* Send NaiadCheckup(), sigh, the problem is the last four bytes is some weird
 * kind of AMF3 object that we can't create with our AMF API
*/
static void checkup(void *priv, uint32_t ts)
{
	struct _mayhem *m = priv;
	static const uint8_t buf[] = {
		0x00, 0x02, 0x00, 0x0c,  'N',  'a',  'i',  'a',
		 'd',  'C',  'h',  'e',  'c',  'k',  'u',  'p',
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x05, 0x11, 0x09, 0x01, 0x01
	};
	rtmp_send(m->rtmp, 3, 0, ts,
			RTMP_MSG_FLEX_MESSAGE, buf, sizeof(buf));
}

mayhem_t mayhem_connect(wmvars_t vars)
{
	struct _mayhem *m;
	static const struct rtmp_ops ops = {
		.invoke = dispatch,
		.notify = notify,
		.audio = rip,
		.video = rip,
		.stream_start = stream_start,
		.read_report_sent = checkup,
	};

	m = calloc(1, sizeof(*m));
	if ( NULL == m )
		goto out;

	m->sid = vars->sid;

	m->rtmp = rtmp_connect(vars->tcUrl);
	if ( NULL == m->rtmp )
		goto out_free;

	m->ns = netstatus_new(m->rtmp, 3, 0);
	if ( NULL == m->ns )
		goto out_free;

	rtmp_set_handlers(m->rtmp, &ops, m);

	if ( !invoke_connect(m, vars) )
		goto out_free_netstatus;

	while ( m->state != MAYHEM_STATE_ABORT && rtmp_pump(m->rtmp) ) {
		switch(m->state) {
		case MAYHEM_STATE_CONNECTING:
			/* I don't sleep, I wait */
			break;
		case MAYHEM_STATE_FROZEN:
			printf("mayhem: fuck this shit!\n");
			mayhem_abort(m);
			break;
		case MAYHEM_STATE_AUTHORIZED:
			create_stream(m, 2.0);
			break;
		case MAYHEM_STATE_GOT_STREAM:
			play(m);
			break;
		case MAYHEM_STATE_PLAYING:
			/* let the good times roll */
			break;
		default:
			printf("ugh? %d\n", m->state);
			abort();
		}
	}

	/* success */
	goto out;
out_free_netstatus:
	netstatus_free(m->ns);
out_free:
	free(m);
	m = NULL;
out:
	return m;
}
