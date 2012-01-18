#include <wmdump.h>
#include <blob.h>
#include <os.h>
#include <rtmp/amf.h>

static int do_file(const char *fn)
{
	invoke_t inv;
	uint8_t *b;
	size_t sz;
	int ret = 0;

	b = blob_from_file(fn, &sz);
	if ( NULL == b )
		goto out;

	inv = amf_invoke_from_buf(b, sz);
	if ( NULL == inv )
		goto out_free;

	printf("%s:\n", fn);
	amf_invoke_pretty_print(inv);

	ret = 1;
out_free:
	blob_free(b, sz);
out:
	return ret;
}

int main(int argc, char **argv)
{
	int i;

	setvbuf(stdout, (char *) NULL, _IOLBF, 0);

	if ( argc < 2 ) {
		fprintf(stderr, "Usage:\t%s <bin...>\n", argv[0]);
		return EXIT_FAILURE;
	}

	for(i = 1; i < argc; i++)
		do_file(argv[i]);
	return EXIT_SUCCESS;
}
