#include <liblouis.h>

int lou_translate(
  const char *tableList,
  const widechar *inbuf,
  int *inlen,
  widechar *outbuf,
  int *outlen,
  formtype *typeform,
  char *spacing,
  int *outputPos,
  int *inputPos,
  int *cursorPos,
  int mode);