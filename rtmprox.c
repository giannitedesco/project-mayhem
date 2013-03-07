#include <wmdump.h>
#include <rtmp/amf.h>
#include <os.h>
#include <list.h>
#include <nbio.h>
#include <rtmp/rtmp.h>
#include <rtmp/proto.h>
#include <rtmp/rtmpd.h>

static const char *cmd;

static int ctor(rtmpd_t r, void *listener_priv)
{
	printf("%s: ctor\n", cmd);
	return 1;
}

static void pkt(rtmpd_t r, struct rtmp_pkt *pkt,
		const uint8_t *buf, size_t sz)
{
	invoke_t inv;

	printf(".id = %d (0x%x)\n", pkt->chan, pkt->chan);
	printf(".dest = %d (0x%x)\n", pkt->dest, pkt->dest);
	printf(".ts = %d (0x%x)\n", pkt->ts, pkt->ts);
	printf(".sz = %zu\n", sz);
	printf(".type = %d (0x%x)\n", pkt->type, pkt->type);

	switch(pkt->type) {
	case RTMP_MSG_NOTIFY:
	case RTMP_MSG_INVOKE:
		inv = amf_invoke_from_buf(buf, sz);
		if ( inv ) {
			amf_invoke_pretty_print(inv);
			amf_invoke_free(inv);
			break;
		}
		/* fall through */
	default:
		hex_dump(buf, sz, 16);
		break;
	}
}

static void conn_reset(rtmpd_t r, const char *str)
{
	printf("%s: conn_reset: %s\n", cmd, str);
	rtmpd_close(r);
}

static void dtor(rtmpd_t r)
{
	printf("%s: dtor\n", cmd);
}

int main(int argc, char **argv)
{
	struct iothread iothread;
	struct eventloop *plugin = NULL;
	int port = 12345;
	static const struct rtmpd_ops ops = {
		.ctor = ctor,
		.pkt = pkt,
		.conn_reset = conn_reset,
		.dtor = dtor,
	};

	setvbuf(stdout, (char *) NULL, _IOLBF, 0);

	cmd = argv[0];
	if ( argc > 1 ) {
		port = atoi(argv[1]);
		if ( port < 0 ) {
			fprintf(stderr, "%s: bad port: '%s'\n", cmd, argv[1]);
			return EXIT_FAILURE;
		}
	}

	if ( !sock_init(1) )
		return EXIT_FAILURE;

	if ( !nbio_init(&iothread, plugin) )
		return EXIT_FAILURE;

	printf("%s: Using %s eventloop plugin\n",
		cmd, iothread.plugin->name);
	rtmp_listen(&iothread, NULL, port, &ops, NULL);

	do{
		nbio_pump(&iothread, -1);
	}while( !list_empty(&iothread.active) );

	sock_fini();

	return EXIT_SUCCESS;
}
