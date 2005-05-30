#include "internal.h"

#include <string.h>
#include <glib.h>

#include "privacy.h"
#include "util.h"
#include "debug.h"

char *return_string_between(const char *startbit, const char *endbit,
                            const char *source);

void gaym_convert_nick_to_gaycom(char *);

void convert_nick_from_gaycom(char *);

gchar *ascii2native(const gchar * str);

gboolean gaym_privacy_check(GaimConnection * gc, const char *nick);

// vim:tabstop=4:shiftwidth=4:expandtab:
