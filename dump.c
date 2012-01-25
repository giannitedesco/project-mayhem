#include <wmdump.h>
#include <wmvars.h>
#include <rtmp/amf.h>
#include <rtmp/rtmp.h>
#include <mayhem.h>
#include <flv.h>

#include <time.h>

static const char *cmd;

struct dumper {
	mayhem_t mayhem;
	struct naiad_room room;
	char *bitch;
	FILE *flv;
};

static int update_bitch(struct dumper *d, const char *bitch)
{
	char *b = strdup(bitch);
	if ( NULL == b )
		return 0;
	free(d->bitch);
	d->bitch = b;
	return 1;
}

static int update_topic(struct naiad_room *r, const struct naiad_room *new)
{
	char *t = strdup(new->topic);
	if ( NULL == t )
		return 0;
	free((char *)r->topic);
	r->topic = t;
	r->flags = new->flags;
	return 1;
}

static void NaiadAuthorize(void *priv, int code,
				const char *nick,
				const char *bitch,
				unsigned int sid,
				struct naiad_room *room)
{
	struct dumper *d = priv;
	printf("NaiadAuthorize: code = %u\n", code);
	printf(" your nickname: %s\n", nick);
	printf(" performer: %s\n", bitch);
	printf(" room flags: %d (0x%x)\n", room->flags, room->flags);
	printf(" topic is: %s\n", (room->topic) ? room->topic : "");
	update_bitch(d, bitch);
	update_topic(&d->room, room);
}

static void NaiadFreeze(void *priv, int code, void *u1,
				int u2, const char *desc)
{
	printf("NaiadFreeze: %d: %s\n", code, desc);
}

static void rip(void *priv, struct rtmp_pkt *pkt,
			const uint8_t *buf, size_t sz)
{
	struct dumper *d = priv;
	flv_rip(d->flv, pkt, buf, sz);
}

static void play(void *priv)
{
	struct dumper *d = priv;
	char buf[((d->bitch) ? strlen(d->bitch) : 64) + 128];
	char tmbuf[128];
	struct tm *tm;
	time_t t;

	t = time(NULL);

	tm = localtime(&t);
	strftime(tmbuf, sizeof(tmbuf), "%F-%H-%M-%S", tm);
	snprintf(buf, sizeof(buf), "%s-%s.flv",
		(d->bitch) ? d->bitch : "UNKNOWN", tmbuf);

	flv_close(d->flv);
	d->flv = flv_creat(buf);

	printf("%s: create FLV: %s\n", cmd, buf);
}

static void stop(void *priv)
{
	struct dumper *d = priv;
	printf("%s: stop\n", cmd);
	flv_close(d->flv);
	d->flv = NULL;
}

static void reset(void *priv)
{
	struct dumper *d = priv;
	printf("%s: reset\n", cmd);
	flv_close(d->flv);
	d->flv = NULL;
}

static int wmdump(const char *varfile)
{
	static const struct mayhem_ops ops = {
		.NaiadAuthorize = NaiadAuthorize,
		.NaiadFreeze = NaiadFreeze,

		.stream_play = play,
		.stream_reset = reset,
		.stream_stop = stop,
		.stream_packet = rip,
	};
	struct dumper d;
	wmvars_t v;
	int ret = 0;

	v = wmvars_parse(varfile);
	if ( NULL == v )
		goto out;

	memset(&d, 0, sizeof(d));
	d.mayhem = mayhem_connect(v, &ops, &d);
	if ( NULL == d.mayhem )
		goto out_free_vars;

	while( mayhem_pump(d.mayhem) )
		/* do nothing */;

	printf("success\n");
	ret = 1;

	flv_close(d.flv);
	free(d.bitch);
	free((char *)d.room.topic);
	mayhem_close(d.mayhem);
out_free_vars:
	wmvars_free(v);
out:
	return ret;
}

int main(int argc, char **argv)
{
	setvbuf(stdout, (char *) NULL, _IOLBF, 0);

	if ( argc < 2 ) {
		fprintf(stderr, "Usage:\t%s <varfile>\n", argv[0]);
		return EXIT_FAILURE;
	}

	cmd = argv[0];
	if ( !wmdump(argv[1]) )
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}
