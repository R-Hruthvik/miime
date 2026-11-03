#ifndef MIIME_SCHEMA_H
#define MIIME_SCHEMA_H

#include "annotate.h"
#include <glib.h>

/* Serialize an annotation stack to a JSON string (caller frees with g_free). */
char *miime_schema_serialize(const MiimeAnnotationStack *stack);
char *miime_schema_serialize_render_config(const MiimeStyleConfig *config);
char *miime_schema_serialize_with_config(const MiimeAnnotationStack *stack,
                                         const MiimeStyleConfig *config);
gboolean miime_schema_deserialize_render_config(const char *json,
                                                MiimeStyleConfig *out,
                                                GError **error);

/* Deserialize a JSON string into a new annotation stack. Returns NULL on error. */
MiimeAnnotationStack *miime_schema_deserialize(const char *json, GError **error);

/* Deserialize annotations and resolve any semantic targets from grounding JSON. */
MiimeAnnotationStack *miime_schema_deserialize_ex(const char *json, GError **error,
                                                   const char *registry_json);

/* Find an element's integer id and bounds in a grounding registry. */
gboolean miime_som_lookup_json(const char *registry_json, int som_id,
                               int *out_x, int *out_y, int *out_w, int *out_h);

/* Validate JSON without building a stack. Returns TRUE if valid. */
gboolean miime_schema_validate(const char *json, GError **error);

#endif /* MIIME_SCHEMA_H */
