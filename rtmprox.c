#include <wmdump.h>
#include <rtmp/amf.h>
#include <os.h>
#include <list.h>
#include <nbio.h>
#include <rtmp/rtmp.h>
#include <rtmp/rtmpd.h>

static const char *cmd;

int main(int argc, char **argv)
{
	struct iothread iothread;
	struct eventloop *plugin = NULL;
	int port = 12345;

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
	rtmp_listen(&iothread, NULL, port, NULL);

	do{
		nbio_pump(&iothread, -1);
	}while( !list_empty(&iothread.active) );

	sock_fini();

	return EXIT_SUCCESS;
}
