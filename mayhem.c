/* This file is part of wmdump
 * Copyright (c) 2011 Gianni Tedesco
 * Released under the terms of GPLv3
*/
#include <wmdump.h>
#include <wmvars.h>

#include <os.h>
#include <list.h>
#include <nbio.h>
#include <rtmp/amf.h>
#include <rtmp/rtmp.h>
#include <rtmp/proto.h>
#include <rtmp/netstatus.h>
#include <mayhem.h>

#include "cvars.h"
#include "mayhem-amf.h"

#define MAYHEM_STATE_ABORT		0
#define MAYHEM_STATE_CONNECTING		1
#define MAYHEM_STATE_CONNECTED		2
#define MAYHEM_STATE_FROZEN		3
#define MAYHEM_STATE_AUTHORIZED		4
#define MAYHEM_STATE_GOT_STREAM		5
#define MAYHEM_STATE_PLAYING		6
#define MAYHEM_STATE_PAUSED		7

struct _mayhem {
	rtmp_t rtmp;
	netstatus_t ns;
	wmvars_t vars;
	char *vidpath;
	const struct mayhem_ops *ops;
	void *priv;
	unsigned int state;
	unsigned int sid;
};

static int is_premium(struct _mayhem *m)
{
	return (m->vars->sakey && strlen(m->vars->sakey));
}

void mayhem_abort(mayhem_t m)
{
	m->state = MAYHEM_STATE_ABORT;
	rtmp_close(m->rtmp);
	netstatus_free(m->ns);
	m->ns = NULL;
	m->rtmp = NULL;
}

static int auth_result(mayhem_t m, float code)
{
	invoke_t inv;
	int ret = 0;

	inv = amf_invoke_new(4);
	if ( NULL == inv ) {
		return 0;
	}

	if ( !amf_invoke_set(inv, 0, amf_string("_result")) )
		goto out;
	if ( !amf_invoke_set(inv, 1, amf_number(code)) )
		goto out;
	if ( !amf_invoke_set(inv, 2, amf_null()) )
		goto out;
	if ( !amf_invoke_set(inv, 3, amf_null()) )
		goto out;

	ret = rtmp_flex_invoke(m->rtmp, 3, 0, inv);
out:
	amf_invoke_free(inv);
	return ret;
}

static int i_auth(mayhem_t m, invoke_t inv)
{
	amf_t o_rc, o_nick, o_bitch, o_sid, o_room;
	struct naiad_room room;

	if ( amf_invoke_nargs(inv) < 26 ) {
		printf("mayhem: too few args in NaiadAuthorize\n");
		return 0;
	}

	o_rc = amf_invoke_get(inv, 1);
	/* [2] null */
	/* [3] sakey */
	/* [4] show number like f/sth/show_%d.mp4 */
	/* [5] unknown number */
	o_nick = amf_invoke_get(inv, 6);
	o_bitch = amf_invoke_get(inv, 7);
	/* [8] number */
	/* [9] empty string */
	/* [10] number a bit larger than show number */
	/* [11] bool */
	o_sid = amf_invoke_get(inv, 12); /* or mp4 path */
	/* [13] type GUEST = 0, PREMIUM = 1, RECORDED = 2,
	 *      FULLSCREEN = 3, EXCLUSIVE = 4, BLOCK_PREMIUM = 5,
	 *      BLOCK_EXCLUSIVE = 6
	 */
	/* bullshit */
	/* [20] prebill */
	/* [21] maxgold */
	/* [22] mingold */
	/* [23] exchangerate */
	/* [24] currencyhtml */
	o_room = amf_invoke_get(inv, 25);

	if ( amf_type(o_rc) != AMF_NUMBER ||
			amf_type(o_nick) != AMF_STRING ||
			amf_type(o_bitch) != AMF_STRING ||
			amf_type(o_sid) != AMF_STRING ||
			amf_type(o_room) != AMF_OBJECT ) {
		printf("mayhem: type mismatch in NaiadAuthorize args\n");
		return 0;
	}

	room.topic = amf_object_get_string(o_room, "roomtopic", NULL);
	room.flags = amf_object_get_number(o_room, "flags", 0);

	/* XXX: Another way to get vid path, not via NaiadSet */
#if 0
	path = amf_get_string(o_sid);
#endif

	m->state = MAYHEM_STATE_AUTHORIZED;
	if ( m->ops && m->ops->NaiadAuthorize )
		(m->ops->NaiadAuthorize)(m->priv,
				amf_get_number(o_rc),
				amf_get_string(o_nick),
				amf_get_string(o_bitch),
				atoi(amf_get_string(o_sid)),
				&room);

	/* For a pay-show, result is expected */
	if ( is_premium(m) ) {
		if ( !auth_result(m, amf_get_number(o_rc)) )
			mayhem_abort(m);
	}

	return 1;
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

	m->state = MAYHEM_STATE_FROZEN;
	if ( m->ops && m->ops->NaiadFreeze )
		(*m->ops->NaiadFreeze)(m->priv, amf_get_number(o_rc),
				NULL, -1, amf_get_string(o_desc));
	return 1;
}

static int user_parse(amf_t obj, struct naiad_user *usr)
{
	if ( NULL == obj || amf_type(obj) != AMF_OBJECT )
		return 0;

	memset(usr, 0, sizeof(*usr));
	usr->flags = amf_object_get_number(obj, "flags", 0);
	usr->id = amf_object_get_string(obj, "id", NULL);
	usr->name = amf_object_get_string(obj, "userName", NULL);

	return 1;
}

/* These are weird, we seem to get empty ones, don't know what 'ac' means,
 * must be incrementally updated somehow.
*/
static int i_userlist(mayhem_t m, invoke_t inv)
{
	unsigned int i, nargs, ac, nusr;
	struct naiad_user *usr;
	amf_t obj;

	nargs = amf_invoke_nargs(inv);
	if ( nargs < 5 ) {
		printf("mayhem: too few args in NaiadUserList\n");
		return 0;
	}

	obj = amf_invoke_get(inv, nargs - 1);
	ac = amf_object_get_number(obj, "ac", 0);

	dprintf("mayhem: user list: %d\n", ac);

	nusr = nargs - 5;
	usr = calloc(nusr, sizeof(*usr));
	if ( NULL == usr )
		return 0;

	for(i = 0; i < nusr; i++) {
		amf_t obj;

		obj = amf_invoke_get(inv, 3 + i);
		if ( !user_parse(obj, &usr[i]) )
			continue;

		dprintf(" - %s ('%s', %d)\n",
			usr[i].name, usr[i].id, usr[i].flags);
	}

	if ( m->ops && m->ops->NaiadUserList )
		(m->ops->NaiadUserList)(m->priv, ac, usr, nusr);
	free(usr);
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

	if ( m->ops && m->ops->NaiadAddChat )
		(m->ops->NaiadAddChat)(m->priv,
					amf_get_string(user),
					amf_get_string(chat));
	return 1;
}

/* NaiadPreGoldShow */
static int i_pregold(mayhem_t m, invoke_t inv)
{
	struct naiad_goldshow gs;
	amf_t obj;

	if ( amf_invoke_nargs(inv) < 4 ) {
		printf("mayhem: too few args in NaiadPreGoldShow\n");
		return 0;
	}

	obj = amf_invoke_get(inv, 3);
	if ( NULL == obj || amf_type(obj) != AMF_OBJECT ) {
		printf("mayhem: type mismatch in NaiadPreGoldShow\n");
		return 0;
	}

	memset(&gs, 0, sizeof(gs));
	gs.duration = amf_object_get_number(obj, "duration", 0);
	gs.id = amf_object_get_number(obj, "id", 0);
	gs.maxwait = amf_object_get_number(obj, "maxwait", 0);
	gs.minbuyin = amf_object_get_number(obj, "minbuyin", 0);
	gs.pledged = amf_object_get_number(obj, "pledged", 0);
	gs.pledgedamt = amf_object_get_number(obj, "pledgedamt", 0);
	gs.requestedamt = amf_object_get_number(obj, "requestedamt", 0);
	gs.showtopic = amf_object_get_string(obj, "showtopic", NULL);
	gs.timetostart = amf_object_get_number(obj, "timetostart", 0);
	gs.total = amf_object_get_number(obj, "total", 0);

	if ( m->ops && m->ops->NaiadPreGoldShow )
		(m->ops->NaiadPreGoldShow)(m->priv, &gs);

	return 1;
}

/* NaiadPledgeGold(number, null, {.amount = number, .status = number} */
static int i_pledge_gold(mayhem_t m, invoke_t inv)
{
	double a, s;
	amf_t obj;

	if ( amf_invoke_nargs(inv) < 4 ) {
		printf("mayhem: too few args in NaiadPledgeGold\n");
		return 0;
	}

	obj = amf_invoke_get(inv, 3);
	if ( NULL == obj || amf_type(obj) != AMF_OBJECT ) {
		printf("mayhem: bad argument\n");
		return 0;
	}

	a = amf_object_get_number(obj, "amount", 0);
	s = amf_object_get_number(obj, "status", 0);

	if ( m->ops && m->ops->NaiadPledgeGold )
		(m->ops->NaiadPledgeGold)(m->priv, a, s);
	return 1;
}

/* NaiadSet(number = 0, null, string path, number = 1) */
static int i_set(mayhem_t m, invoke_t inv)
{
	amf_t o_vidpath;

	if ( !is_premium(m) )
		printf("mayhem: NaiadSet: on non premium, weird?\n");

	o_vidpath = amf_invoke_get(inv, 3);
	if ( amf_type(o_vidpath) != AMF_STRING ) {
		printf("mayhem: unable to get vidpath in NaiadSet\n");
	}else{
		char *path;
		path = strdup(amf_get_string(o_vidpath));
		if ( path ) {
			free(m->vidpath);
			m->vidpath = path;
		}
	}

	if ( !netstatus_createstream(m->ns, 2.0) ) {
		mayhem_abort(m);
		return 1;
	}

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
		{.method = "NaiadPledgeGold", .call = i_pledge_gold},
		{.method = "NaiadPreGoldShow", .call = i_pregold},
		{.method = "NaiadSet", .call = i_set},
		/* NaiadGoldShow */
		/* NaiadQualityChanged */
		/* NaiadPause */
		/* NaiadPlay */

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

/* RTMP Stream event callbacks */
static int invoke(void *priv, invoke_t inv)
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
	return 1;
}

static int invoke_connect(struct _mayhem *m)
{
	invoke_t inv;
	int ret = 0;

	if ( is_premium(m) )
		printf("mayhem: attempting PREMIUM login\n");

	inv = mayhem_amf_connect(m->vars, is_premium(m));
	if ( NULL == inv )
		goto out;
	ret = netstatus_connect_custom(m->ns, inv);
	if ( ret ) {
		m->state = MAYHEM_STATE_CONNECTING;
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
		/* ?? */
	}
	amf_invoke_free(inv);
out:
	return ret;
}

static int notify(void *priv, struct rtmp_pkt *pkt,
			const uint8_t *buf, size_t sz)
{
	struct _mayhem *m = priv;
	invoke_t inv;
	amf_t a;
	int ret = 0;

	inv = amf_invoke_from_buf(buf, sz);
	if ( NULL == inv )
		goto out;

	if ( amf_invoke_nargs(inv) < 1 )
		goto out_free;

	a = amf_invoke_get(inv, 0);
	if ( NULL == a || amf_type(a) != AMF_STRING )
		goto out_free;

	if ( strcmp(amf_get_string(a), "onMetaData") )
		goto out_free;

	printf("Video metadata:\n");
	amf_invoke_pretty_print(inv);

	if ( m->ops && m->ops->stream_packet )
		(*m->ops->stream_packet)(m->priv, pkt, buf, sz);

	ret = 1;

out_free:
	amf_invoke_free(inv);
out:
	return ret;
}

int mayhem_snd_chat(mayhem_t m, const char *msg)
{
	invoke_t inv;
	int ret = 0;

	inv = amf_invoke_new(10);
	if ( NULL == inv ) {
		return 0;
	}

	if ( !amf_invoke_set(inv, 0, amf_string("NaiadSndChat")) )
		goto out;
	if ( !amf_invoke_set(inv, 1, amf_number(0.0)) )
		goto out;
	if ( !amf_invoke_set(inv, 2, amf_null()) )
		goto out;
	if ( !amf_invoke_set(inv, 3, amf_number(0.0)) )
		goto out;
	if ( !amf_invoke_set(inv, 4, amf_string("")) )
		goto out;
	if ( !amf_invoke_set(inv, 5, amf_string(msg)) )
		goto out;
	if ( !amf_invoke_set(inv, 6, amf_string("")) )
		goto out;
	if ( !amf_invoke_set(inv, 7, amf_bool(0)) )
		goto out;
	if ( !amf_invoke_set(inv, 8, amf_string("")) )
		goto out;
	if ( !amf_invoke_set(inv, 9, amf_stringf("<P ALIGN=\"LEFT\">"
				"<FONT FACE=\"Arial\" SIZE=\"11\" "
				"COLOR=\"#000000\" "
				"LETTERSPACING=\"0\" "
				"KERNING=\"0\">%s</FONT></P>", msg)) )
		goto out;

	ret = rtmp_flex_invoke(m->rtmp, 3, 0, inv);
out:
	amf_invoke_free(inv);
	return ret;
}

static int rip(void *priv, struct rtmp_pkt *pkt,
			const uint8_t *buf, size_t sz)
{
	struct _mayhem *m = priv;
	if ( m->ops && m->ops->stream_packet )
		(*m->ops->stream_packet)(m->priv, pkt, buf, sz);
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

/* NetConnection callbacks */
static void stream_connected(netstatus_t ns, void *priv)
{
	struct _mayhem *m = priv;

	m->state = MAYHEM_STATE_CONNECTED;
	if ( !is_premium(m) ) {
		if ( !netstatus_createstream(m->ns, 2.0) ) {
			mayhem_abort(m);
			return;
		}
	}
}

static void stream_created(netstatus_t ns, void *priv, unsigned int stream_id)
{
	struct _mayhem *m = priv;
	amf_t path;

	m->state = MAYHEM_STATE_GOT_STREAM;

	if ( is_premium(m) ) {
		path = amf_string(m->vidpath);
		printf("path is %s\n", amf_get_string(path));
	}else{
		path = amf_stringf("%d", m->sid);
	}

	if ( !netstatus_play(m->ns, path, stream_id) ) {
		mayhem_abort(m);
		return;
	}
}

static void connect_error(netstatus_t ns, void *priv,
			const char *code, const char *desc)
{
	struct _mayhem *m = priv;
	if ( m->ops && m->ops->connect_error )
		(*m->ops->connect_error)(m->priv, code, desc);
	mayhem_abort(m);
}

/* Netsream callbacks */
static void start(netstatus_t ns, void *priv)
{
	struct _mayhem *m = priv;
	invoke_start(m);
	if ( m->ops && m->ops->stream_play )
		(*m->ops->stream_play)(m->priv);
}

static void stop(netstatus_t ns, void *priv)
{
	struct _mayhem *m = priv;
	if ( m->ops && m->ops->stream_stop )
		(*m->ops->stream_stop)(m->priv);
}

static void reset(netstatus_t ns, void *priv)
{
	struct _mayhem *m = priv;
	if ( m->ops && m->ops->stream_reset )
		(*m->ops->stream_reset)(m->priv);
}

static void play_error(netstatus_t ns, void *priv,
			const char *code, const char *desc)
{
	struct _mayhem *m = priv;
	if ( m->ops && m->ops->stream_error)
		(*m->ops->stream_error)(m->priv, code, desc);
	mayhem_abort(m);
}

static void rtmp_connected(void *priv)
{
	static const struct netstream_ops ns_ops = {
		.start = start,
		.stop = stop,
		.reset = reset,
		.error = play_error,
	};
	static const struct netconn_ops nc_ops = {
		.connected = stream_connected,
		.stream_created = stream_created,
		.error = connect_error,
	};
	struct _mayhem *m = priv;

	m->ns = netstatus_new(m->rtmp, 3, 0);
	if ( NULL == m->ns )
		goto out_free_rtmp;
	netstatus_stream_ops(m->ns, &ns_ops, m);
	netstatus_connect_ops(m->ns, &nc_ops, m);

	if ( !invoke_connect(m) )
		goto out_free_netstatus;

	return;
out_free_netstatus:
	netstatus_free(m->ns);
out_free_rtmp:
	rtmp_close(m->rtmp);
}

static void rtmp_died(void *priv, const char *reason)
{
	struct _mayhem *m = priv;
	if ( m->ops && m->ops->connect_error )
		(*m->ops->connect_error)(m->priv, "rtmp", reason);
	mayhem_abort(m);
}

/* Constructor/destructor */
void mayhem_close(mayhem_t m)
{
	if ( m ) {
		netstatus_free(m->ns);
		rtmp_close(m->rtmp);
		free(m->vidpath);
		free(m);
	}
}

mayhem_t mayhem_connect(struct iothread *t, wmvars_t vars,
			const struct mayhem_ops *ops, void *priv)
{
	struct _mayhem *m;
	static const struct rtmp_ops rtmp_ops = {
		.invoke = invoke,
		.notify = notify,
		.audio = rip,
		.video = rip,
		.read_report_sent = checkup,
		.conn_reset = rtmp_died,
		.connected = rtmp_connected,
	};

	m = calloc(1, sizeof(*m));
	if ( NULL == m )
		goto out;

	m->ops = ops;
	m->priv = priv;
	m->vars = vars;
	m->sid = vars->sid;

	m->rtmp = rtmp_connect(t, vars->tcUrl, &rtmp_ops, m);
	if ( NULL == m->rtmp )
		goto out_free;

	/* success */
	goto out;

out_free:
	free(m);
	m = NULL;
out:
	return m;
}
