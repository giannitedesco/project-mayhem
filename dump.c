#include <wmdump.h>
#include <wmvars.h>
#include <mayhem.h>

static const char *cmd;

static int wmdump(const char *varfile)
{
	wmvars_t v;
	mayhem_t m;
	int ret = 0;

	v = wmvars_parse(varfile);
	if ( NULL == v )
		goto out;

	m = mayhem_connect(v);
	if ( NULL == m )
		goto out_free_vars;

	while( mayhem_pump(m) )
		/* do nothing */;

	printf("success\n");
	ret = 1;

	mayhem_close(m);
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
