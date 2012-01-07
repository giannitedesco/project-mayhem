#include <wmdump.h>
#include <wmvars.h>

static const char *cmd;

static int wmdump(const char *varfile)
{
	wmvars_t v;

	v = wmvars_parse(varfile);
	if ( NULL == v )
		return 0;

	wmvars_free(v);
	return 1;
}

int main(int argc, char **argv)
{
	if ( argc < 2 ) {
		fprintf(stderr, "Usage:\t%s <varfile>\n", argv[0]);
		return EXIT_FAILURE;
	}

	cmd = argv[0];
	if ( !wmdump(argv[1]) )
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}
