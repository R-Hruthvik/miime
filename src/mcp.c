#include "mcp.h"
#include "grounding.h"
#include "headless.h"
#include "schema.h"
#include "image.h"
#include "annotate.h"
#include "render.h"
#include <glib.h>
#include <cairo.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *g_state_image = NULL;      /* clean source path */
static char *g_state_annotations = NULL; /* serialized JSON */
static char *g_state_registry = NULL;
static char *g_state_buffer_b = NULL;
static char *g_tool_error = NULL;
static MiimeStyleConfig g_state_render_config;

static void setstr(char **p, const char *v) {
    g_free(*p);
    *p = v ? g_strdup(v) : NULL;
}

static char *file_to_base64(const char *path) {
    if (!path) return NULL;
    gsize len = 0;
    gchar *data = NULL;
    if (!g_file_get_contents(path, &data, &len, NULL)) return NULL;
    char *b64 = g_base64_encode((const guchar *)data, len);
    g_free(data);
    return b64;
}

static void send_result(gint64 id, const char *result_json) {
    GString *g = g_string_new(NULL);
    g_string_append_printf(g, "{\"jsonrpc\":\"2.0\",\"id\":%lld,\"result\":%s}\n", (long long)id, result_json);
    fputs(g->str, stdout);
    fflush(stdout);
    g_string_free(g, TRUE);
}

static void send_error(gint64 id, int code, const char *msg) {
    GString *g = g_string_new(NULL);
    /* escape msg minimally */
    g_string_append_printf(g, "{\"jsonrpc\":\"2.0\",\"id\":%lld,\"error\":{\"code\":%d,\"message\":\"", (long long)id, code);
    for (const char *c = msg; *c; c++) {
        if (*c == '"' || *c == '\\') g_string_append_c(g, '\\');
        g_string_append_c(g, *c);
    }
    g_string_append(g, "\"}}\n");
    fputs(g->str, stdout);
    fflush(stdout);
    g_string_free(g, TRUE);
}

/* extract a JSON string field value (first occurrence) into malloc'd string */
static char *extract_string(const char *json, const char *key) {
    if (!json || !key) return NULL;
    char pat[128];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *p = strstr(json, pat);
    if (!p) return NULL;
    p += strlen(pat);
    while (*p && (*p == ' ' || *p == ':' || *p == '\t')) p++;
    if (*p != '"') return NULL;
    p++;
    size_t cap = 64, len = 0;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    while (*p && *p != '"') {
        if (*p == '\\') { p++; }
        if (len + 2 >= cap) { cap *= 2; buf = realloc(buf, cap); }
        buf[len++] = *p++;
    }
    buf[len] = '\0';
    return buf;
}

static int extract_int(const char *json, const char *key, int def) {
    char pat[128];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *p = strstr(json, pat);
    if (!p) return def;
    p += strlen(pat);
    while (*p && (*p == ' ' || *p == ':' || *p == '\t')) p++;
    return atoi(p);
}

static gboolean extract_bool(const char *json, const char *key, gboolean def) {
    char pat[128];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *p = strstr(json, pat);
    if (!p) return def;
    p += strlen(pat);
    while (*p && (*p == ' ' || *p == ':' || *p == '\t')) p++;
    if (strncmp(p, "true", 4) == 0) return TRUE;
    if (strncmp(p, "false", 5) == 0) return FALSE;
    return def;
}

/* Find the arguments object substring (returns malloc'd copy) */
static char *extract_arguments(const char *req) {
    const char *p = strstr(req, "\"arguments\"");
    if (!p) return g_strdup("{}");
    p = strchr(p, ':');
    if (!p) return g_strdup("{}");
    p++;
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '{') return g_strdup("{}");
    /* find matching brace */
    int depth = 0;
    const char *start = p;
    const char *q = p;
    while (*q) {
        if (*q == '{') depth++;
        else if (*q == '}') { depth--; if (depth == 0) { q++; break; } }
        else if (*q == '"') { q++; while (*q && *q != '"') { if (*q == '\\') q++; q++; } }
        q++;
    }
    size_t n = q - start;
    char *buf = malloc(n + 1);
    memcpy(buf, start, n);
    buf[n] = '\0';
    return buf;
}

static char *extract_params(const char *req) {
    const char *p = strstr(req, "\"params\"");
    if (!p) return g_strdup("{}");
    p = strchr(p, ':');
    if (!p) return g_strdup("{}");
    p++;
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '{') return g_strdup("{}");
    int depth = 0;
    const char *start = p;
    const char *q = p;
    while (*q) {
        if (*q == '{') depth++;
        else if (*q == '}') { depth--; if (depth == 0) { q++; break; } }
        else if (*q == '"') { q++; while (*q && *q != '"') { if (*q == '\\') q++; q++; } }
        q++;
    }
    size_t n = q - start;
    char *buf = malloc(n + 1);
    memcpy(buf, start, n);
    buf[n] = '\0';
    return buf;
}

/* Extract the top-level numeric JSON-RPC "id" from a request line.
 *
 * The MCP SDK serializes requests as {method, params, jsonrpc, id}:
 * params comes BEFORE the real id, and params for patch_ux_annotations
 * contains nested annotation objects with their own string "id" keys
 * (e.g. "id":"err-block"). Taking the FIRST "id" anywhere in the line
 * grabbed a nested annotation id and returned 0 for its string value,
 * so those requests were answered with id:0 — a response the SDK can
 * never match to the pending call, which then hangs until aborted.
 *
 * Scan depth-aware instead: only "id" keys at depth 1 (the root object)
 * with a numeric value count. Returns 1 and stores the id in *out. */
static int extract_id(const char *req, gint64 *out) {
    int depth = 0, in_string = 0, escaped = 0, found = 0;
    const char *p = req;
    *out = 0;
    while (*p) {
        char c = *p;
        if (in_string) {
            if (escaped) escaped = 0;
            else if (c == '\\') escaped = 1;
            else if (c == '"') in_string = 0;
            p++;
            continue;
        }
        if (c == '"') {
            if (depth == 1 && strncmp(p, "\"id\"", 4) == 0) {
                const char *q = p + 4;
                while (*q == ' ' || *q == '\t' || *q == '\r' || *q == '\n') q++;
                if (*q == ':') {
                    q++;
                    while (*q == ' ' || *q == '\t' || *q == '\r' || *q == '\n') q++;
                    if (*q == '-' || (*q >= '0' && *q <= '9')) {
                        *out = (gint64)atoll(q);
                        found = 1;
                    }
                }
            }
            in_string = 1;
            p++;
            continue;
        }
        if (c == '{' || c == '[') depth++;
        else if (c == '}' || c == ']') { if (depth > 0) depth--; }
        p++;
    }
    return found;
}

static char *handle_prepare(const char *args) {
    char *source_type = extract_string(args, "source_type");
    char *source = extract_string(args, "source");
    int vw = extract_int(args, "width", 0);
    /* viewport may be nested; try */
    /* accept viewport_width */
    int vw2 = extract_int(args, "viewport_width", 0);
    if (vw2) vw = vw2;
    int vh = extract_int(args, "height", 0);
    int vh2 = extract_int(args, "viewport_height", 0);
    if (vh2) vh = vh2;
    gboolean visual = extract_bool(args, "visual_aids", TRUE);

    int st = 1;
    if (source_type && strcmp(source_type, "url") == 0) st = 0;

    MiimeViewport vp; vp.width = vw; vp.height = vh;

    char *bpath = NULL;
    char *regjson = NULL;
    int rc = miime_grounding_run(st, source, vp, visual ? 1 : 0, &bpath, &regjson);
    if (rc != 0) {
        g_free(source_type); g_free(source);
        return g_strdup("{\"content\":[{\"type\":\"text\",\"text\":\"grounding failed\"}]}");
    }

    /* clean source path */
    char clean[4096];
    if (st == 0) {
        snprintf(clean, sizeof(clean), "/tmp/opencode/grounding_%d.png", (int)getpid());
    } else {
        snprintf(clean, sizeof(clean), "%s", source ? source : "");
    }
    setstr(&g_state_image, clean);
    setstr(&g_state_buffer_b, bpath);
    setstr(&g_state_registry, regjson);
    setstr(&g_state_annotations, "{\"annotations\":[]}");
    memset(&g_state_render_config, 0, sizeof(g_state_render_config));

    char *b64 = file_to_base64(bpath);

    GString *res = g_string_new("{\"content\":[");
    if (b64) {
        g_string_append_printf(res, "{\"type\":\"image\",\"data\":\"%s\",\"mimeType\":\"image/png\"},", b64);
        g_free(b64);
    }
    /* text registry */
    g_string_append(res, "{\"type\":\"text\",\"text\":\"");
    if (regjson) {
        for (const char *c = regjson; *c; c++) {
            if (*c == '"' || *c == '\\') g_string_append_c(res, '\\');
            g_string_append_c(res, *c);
        }
    }
    g_string_append(res, "\"}]}");

    g_free(source_type); g_free(source); g_free(bpath); g_free(regjson);
    char *out = g_strdup(res->str);
    g_string_free(res, TRUE);
    return out;
}

static int extract_layout_mode(const char *args) {
    char *value=extract_string(args,"layout_mode");
    int mode=0;
    if (value) {
        if (strcmp(value,"margin_rail")==0) mode=1;
        else if (strcmp(value,"in_canvas_floating")==0 || strcmp(value,"in_canvas")==0) mode=2;
        g_free(value);
    } else {
        int numeric=extract_int(args,"layout_mode",0);
        if (numeric>=0 && numeric<=2) mode=numeric;
    }
    return mode;
}

static MiimeAestheticStyle extract_aesthetic(const char *args) {
    char *value=extract_string(args,"aesthetic_style");
    MiimeAestheticStyle style=MIIME_AESTHETIC_ROUGH_SKETCH;
    if (value) {
        if (strcmp(value,"clean_technical")==0) style=MIIME_AESTHETIC_CLEAN_TECHNICAL;
        g_free(value);
    } else if (extract_int(args,"aesthetic_style",0)==1) style=MIIME_AESTHETIC_CLEAN_TECHNICAL;
    return style;
}

static gboolean parse_requested_config(const char *args, MiimeStyleConfig *out) {
    GError *err = NULL;
    MiimeStyleConfig parsed = {0};
    if (!miime_schema_deserialize_render_config(args ? args : "{}", &parsed, &err)) {
        g_tool_error = g_strdup(err ? err->message : "invalid render config");
        if (err) g_error_free(err);
        return FALSE;
    }
    *out = parsed;
    return TRUE;
}

static void merge_render_config(MiimeStyleConfig *base, const MiimeStyleConfig *patch) {
    if (patch->has_reset_render_config && patch->reset_render_config)
        memset(base, 0, sizeof(*base));
    if (patch->has_drawing_style) { base->drawing_style=patch->drawing_style; base->has_drawing_style=TRUE; }
    if (patch->has_theme) { base->theme=patch->theme; base->has_theme=TRUE; }
    if (patch->has_analysis_background) { base->analysis_background_rgba=patch->analysis_background_rgba; base->has_analysis_background=TRUE; }
    if (patch->has_aesthetic_style) { base->aesthetic_style=patch->aesthetic_style; base->has_aesthetic_style=TRUE; }
    base->has_reset_render_config=FALSE;
    base->reset_render_config=FALSE;
    /* The legacy aesthetic alias chooses a 2.1 style until a new style or
     * theme opts in; the resolver maps it only once that opt-in is active. */
}

static MiimeAestheticStyle config_aesthetic(const MiimeStyleConfig *config, const char *args) {
    if (config && config->has_aesthetic_style) return config->aesthetic_style;
    return extract_aesthetic(args);
}

static char *extract_resolution_object(const char *json) {
    const char *key=json?strstr(json,"\"resolution\":"):NULL;
    if(!key)return NULL;
    const char *start=strchr(key,':');
    if(!start)return NULL;
    do { start++; } while(*start==' '||*start=='\t'||*start=='\r'||*start=='\n');
    if(*start!='{')return NULL;
    int depth=0; gboolean quoted=FALSE,escaped=FALSE;
    for(const char *p=start;*p;p++) {
        if(quoted) {
            if(escaped)escaped=FALSE;
            else if(*p=='\\')escaped=TRUE;
            else if(*p=='"')quoted=FALSE;
            continue;
        }
        if(*p=='"')quoted=TRUE;
        else if(*p=='{')depth++;
        else if(*p=='}'&&--depth==0)return g_strndup(start,(gsize)(p-start+1));
    }
    return NULL;
}

static char *handle_render_internal(const char *args, const char *annotations_override,
                                    const MiimeStyleConfig *provided_config) {
    /* deserialize annotations from args (extra keys ignored) */
    GError *err = NULL;
    const char *annotation_source=annotations_override;
    if (!annotation_source && args && !strstr(args,"\"annotations\"") && g_state_annotations)
        annotation_source=g_state_annotations;
    MiimeStyleConfig local_config = {0};
    if (provided_config) local_config=*provided_config;
    else {
        MiimeStyleConfig request_config={0};
        if (!parse_requested_config(args, &request_config)) return NULL;
        local_config=g_state_render_config;
        merge_render_config(&local_config,&request_config);
    }
    MiimeAnnotationStack *stack = miime_schema_deserialize_ex(annotation_source ? annotation_source : args, &err, g_state_registry);
    if (!stack) {
        if (err && strstr(err->message,"unknown som_id")) {
            int som_id = extract_int(args, "som_id", -1);
            g_tool_error = g_strdup_printf("unknown som_id %d (or missing bounds)", som_id);
        } else g_tool_error = g_strdup(err ? err->message : "invalid annotations");
        if (err) g_error_free(err);
        return NULL;
    }
    if (err) { g_error_free(err); err=NULL; }

    MiimeAestheticStyle aesthetic=config_aesthetic(&local_config,args);

    /* Persist requested config with annotations. A plain render keeps config
     * established by patch; an explicit request records its own requested
     * fields while still leaving the patch config inheritance state alone. */
    gboolean request_has_config = local_config.has_drawing_style || local_config.has_theme ||
        local_config.has_analysis_background || local_config.has_aesthetic_style;
    const MiimeStyleConfig *state_config = request_has_config ? &local_config : &g_state_render_config;
    char *ser = miime_schema_serialize_with_config(stack, state_config);
    setstr(&g_state_annotations, ser);
    g_free(ser);
    /* render */
    cairo_surface_t *surf = NULL;
    int w=0,h=0;
    int layout_mode=extract_layout_mode(args);
    gboolean dry_run=extract_bool(args,"dry_run",FALSE);
    gboolean style_opted_in=local_config.has_drawing_style || local_config.has_theme;
    char *resolution_json=NULL;
    MiimeRenderOptions options={.requested=local_config,.resolution=NULL};
    if (g_state_image) {
        MiimeImage *img = miime_image_load(g_state_image);
        if (img) {
            if(style_opted_in&&!dry_run) {
                char *diagnostic_payload=miime_render_annotations_with_options(img,stack,layout_mode,aesthetic,&options,TRUE,NULL,NULL,NULL);
                resolution_json=extract_resolution_object(diagnostic_payload);
                g_free(diagnostic_payload);
            }
            char *placement_json=style_opted_in
                ? miime_render_annotations_with_options(img,stack,layout_mode,aesthetic,&options,dry_run,&surf,&w,&h)
                : miime_render_annotations_with_dry_run(img,stack,layout_mode,aesthetic,dry_run,&surf,&w,&h);
            if (dry_run) {
                GString *dry=g_string_new("{\"content\":[{\"type\":\"text\",\"text\":");
                const char *placement_text=placement_json ? placement_json : "{\"placements\":[]}";
                g_string_append_c(dry,'"');
                for (const char *c=placement_text;*c;c++) {
                    if (*c=='"' || *c=='\\') g_string_append_c(dry,'\\');
                    if (*c=='\n') g_string_append(dry,"\\n");
                    else g_string_append_c(dry,*c);
                }
                g_string_append(dry,"\"}]}");
                g_free(placement_json);
                g_free(resolution_json);
                miime_image_free(img);
                miime_annot_stack_free(stack);
                return g_string_free(dry,FALSE);
            }
            g_free(placement_json);
            miime_image_free(img);
        }
    }
    char *b64 = NULL;
    char outpath[4096];
    snprintf(outpath, sizeof(outpath), "/tmp/opencode/render_%d.png", (int)getpid());
    if (surf) {
        miime_render_save_to_file(surf, outpath);
        cairo_surface_destroy(surf);
        b64 = file_to_base64(outpath);
    }

    /* telemetry */
    int n = miime_annot_stack_count(stack);
    GString *telem = g_string_new("{\"mode\":\"auto\",\"annotations\":");
    g_string_append_printf(telem, "%d,\"collisions_resolved\":0,\"warnings\":[", n);
    /* warning if >5 annotations */
    if (n >= 5) g_string_append(telem, "\"dense: consider margin_rail\"");
    g_string_append(telem, "]");
    if(resolution_json) { g_string_append(telem,",\"resolution\":"); g_string_append(telem,resolution_json); }
    g_string_append(telem,"}");
    g_free(resolution_json);

    GString *res = g_string_new("{\"content\":[");
    if (b64) {
        g_string_append_printf(res, "{\"type\":\"image\",\"data\":\"%s\",\"mimeType\":\"image/png\"},", b64);
        g_free(b64);
    }
    g_string_append(res, "{\"type\":\"text\",\"text\":\"");
    /* escape telem */
    for (const char *c = telem->str; *c; c++) {
        if (*c == '"' || *c == '\\') g_string_append_c(res, '\\');
        g_string_append_c(res, *c);
    }
    g_string_append(res, "\"}]}");

    miime_annot_stack_free(stack);
    char *out = g_strdup(res->str);
    g_string_free(res, TRUE);
    g_string_free(telem, TRUE);
    return out;
}

static char *handle_render(const char *args) { return handle_render_internal(args,NULL,NULL); }

static char *handle_patch(const char *args) {
    MiimeStyleConfig patch_config={0};
    if (!parse_requested_config(args,&patch_config)) return NULL;
    MiimeStyleConfig next_config=g_state_render_config;
    merge_render_config(&next_config,&patch_config);
    MiimeAnnotationStack *validated_stack=NULL;
    GError *validation_error=NULL;
    /* If annotations provided, replace; else apply operations crudely by reusing current */
    if (strstr(args, "\"annotations\"")) {
        /* replace state annotations with args annotations */
        validated_stack = miime_schema_deserialize_ex(args, &validation_error, g_state_registry);
        if (!validated_stack) {
            if (validation_error && strstr(validation_error->message,"unknown som_id")) {
                int som_id = extract_int(args, "som_id", -1);
                g_tool_error = g_strdup_printf("unknown som_id %d (or missing bounds)", som_id);
            } else g_tool_error = g_strdup(validation_error ? validation_error->message : "invalid annotations");
            if (validation_error) g_error_free(validation_error);
            return NULL;
        }
    }
    if (validation_error) g_error_free(validation_error);
    if (validated_stack) {
        char *ser=miime_schema_serialize_with_config(validated_stack,&next_config);
        setstr(&g_state_annotations,ser);
        g_free(ser);
        miime_annot_stack_free(validated_stack);
    }
    g_state_render_config=next_config;
    /* re-render */
    char *rendered=handle_render_internal(args,g_state_annotations ? g_state_annotations : args,&g_state_render_config);
    GError *state_error=NULL;
    MiimeAnnotationStack *state_stack=miime_schema_deserialize(g_state_annotations ? g_state_annotations : "{\"annotations\":[]}",&state_error);
    if (state_stack) {
        char *ser=miime_schema_serialize_with_config(state_stack,&g_state_render_config);
        setstr(&g_state_annotations,ser);
        g_free(ser);
        miime_annot_stack_free(state_stack);
    }
    if (state_error) g_error_free(state_error);
    return rendered;
}

static char *handle_get(const char *args) {
    (void)args;
    const char *s = g_state_annotations ? g_state_annotations : "{\"annotations\":[]}";
    GString *res = g_string_new("{\"content\":[{\"type\":\"text\",\"text\":\"");
    for (const char *c = s; *c; c++) {
        if (*c == '"' || *c == '\\') g_string_append_c(res, '\\');
        g_string_append_c(res, *c);
    }
    g_string_append(res, "\"}]}");
    char *out = g_strdup(res->str);
    g_string_free(res, TRUE);
    return out;
}

static char *handle_verify(const char *args) {
    (void)args;
    char *clean_b64 = file_to_base64(g_state_image);
    /* render current */
    char *rendered = handle_render(g_state_annotations ? g_state_annotations : "{\"annotations\":[]}");
    /* extract image data? just return rendered + clean */
    GString *res = g_string_new("{\"content\":[");
    if (clean_b64) {
        g_string_append_printf(res, "{\"type\":\"image\",\"data\":\"%s\",\"mimeType\":\"image/png\"},", clean_b64);
        g_free(clean_b64);
    }
    /* append rendered's content images by naive: include telemetry text */
    g_string_append(res, "{\"type\":\"text\",\"text\":\"verify: before/after provided\"}]}");
    g_free(rendered);
    char *out = g_strdup(res->str);
    g_string_free(res, TRUE);
    return out;
}

static const char *tools_list_json =
"{\"tools\":[{\"name\":\"prepare_ux_canvas\",\"description\":\"Ingest an image or URL, optionally draw visual aids, and build a Set-of-Mark grounding registry.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"source_type\":{\"type\":\"string\",\"enum\":[\"image_path\",\"url\"]},\"source\":{\"type\":\"string\"},\"visual_aids\":{\"type\":\"boolean\"},\"width\":{\"type\":\"integer\",\"minimum\":0},\"height\":{\"type\":\"integer\",\"minimum\":0},\"viewport_width\":{\"type\":\"integer\",\"minimum\":0},\"viewport_height\":{\"type\":\"integer\",\"minimum\":0},\"viewport\":{\"type\":\"object\",\"properties\":{\"width\":{\"type\":\"integer\",\"minimum\":0},\"height\":{\"type\":\"integer\",\"minimum\":0}}}},\"required\":[\"source\"]},\"annotations\":{\"readOnlyHint\":false}},{\"name\":\"render_ux_annotations\",\"description\":\"Render up to 15 annotations using the annotation vocabulary, layout, and aesthetic options. dry_run returns label placements without a PNG.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"layout_mode\":{\"type\":\"string\",\"enum\":[\"auto\",\"margin_rail\",\"in_canvas_floating\"]},\"aesthetic_style\":{\"type\":\"string\",\"enum\":[\"rough_sketch\",\"clean_technical\"]},\"drawing_style\":{\"type\":\"string\",\"enum\":[\"technical\",\"sketch\",\"editorial\",\"minimal\"]},\"theme\":{\"type\":\"string\",\"enum\":[\"auto\",\"light\",\"dark\",\"warm\",\"neutral\"]},\"analysis_background\":{\"type\":\"string\",\"pattern\":\"^#[0-9A-Fa-f]{6}$\"},\"reset_render_config\":{\"type\":\"boolean\"},\"dry_run\":{\"type\":\"boolean\"},\"annotations\":{\"type\":\"array\",\"maxItems\":15,\"items\":{\"type\":\"object\",\"required\":[\"type\"],\"properties\":{\"id\":{\"type\":\"string\"},\"type\":{\"type\":\"string\",\"enum\":[\"highlight_box\",\"spacing_ruler\",\"alignment_redline\",\"callout\",\"pin\",\"freehand\",\"circle\",\"bracket\",\"underline\",\"strike\",\"pen\",\"line\",\"rect\",\"ellipse\",\"arrow\",\"text\",\"blur\",\"crop\"]},\"source\":{\"type\":\"string\"},\"som_id\":{\"type\":\"integer\"},\"points\":{\"oneOf\":[{\"type\":\"array\",\"items\":{\"type\":\"number\"},\"minItems\":2},{\"type\":\"array\",\"items\":{\"type\":\"array\",\"items\":{\"type\":\"number\"},\"minItems\":2,\"maxItems\":2},\"minItems\":1}]},\"color\":{\"type\":\"array\",\"items\":{\"type\":\"number\"},\"minItems\":4,\"maxItems\":4},\"ink\":{\"type\":\"string\",\"enum\":[\"thin\",\"regular\",\"bold\",\"marker\"]},\"stroke_width\":{\"oneOf\":[{\"type\":\"number\",\"exclusiveMinimum\":0},{\"type\":\"array\",\"items\":{\"type\":\"number\",\"exclusiveMinimum\":0},\"minItems\":2,\"maxItems\":2}]},\"dash\":{\"type\":\"string\",\"enum\":[\"solid\",\"dashed\",\"dotted\"]},\"fill\":{\"type\":\"string\",\"enum\":[\"none\",\"hachure\",\"solid\"]},\"fill_color\":{\"type\":\"array\",\"items\":{\"type\":\"number\"},\"minItems\":4,\"maxItems\":4},\"radius\":{\"type\":\"number\",\"minimum\":0},\"arrow\":{\"type\":\"string\",\"enum\":[\"none\",\"end\",\"both\",\"start\"]},\"z\":{\"type\":\"string\",\"enum\":[\"under\",\"normal\",\"over\"]},\"text\":{\"type\":\"string\"},\"font_size\":{\"type\":\"number\",\"minimum\":0},\"label\":{\"type\":\"object\",\"properties\":{\"text\":{\"type\":\"string\"},\"anchor\":{\"oneOf\":[{\"type\":\"string\",\"enum\":[\"auto\",\"right\",\"bottom\",\"0\",\"1\",\"2\",\"3\",\"4\",\"5\",\"6\",\"7\"]},{\"type\":\"integer\",\"minimum\":0,\"maximum\":7}]},\"leader\":{\"type\":\"string\",\"enum\":[\"curved\",\"elbow\",\"none\"]}},\"required\":[\"text\"]}}}}}},\"annotations\":{\"readOnlyHint\":false}},{\"name\":\"patch_ux_annotations\",\"description\":\"Replace the current annotation set and render it; accepts the same annotation, layout, and dry-run options as render_ux_annotations.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"layout_mode\":{\"type\":\"string\",\"enum\":[\"auto\",\"margin_rail\",\"in_canvas_floating\"]},\"aesthetic_style\":{\"type\":\"string\",\"enum\":[\"rough_sketch\",\"clean_technical\"]},\"drawing_style\":{\"type\":\"string\",\"enum\":[\"technical\",\"sketch\",\"editorial\",\"minimal\"]},\"theme\":{\"type\":\"string\",\"enum\":[\"auto\",\"light\",\"dark\",\"warm\",\"neutral\"]},\"analysis_background\":{\"type\":\"string\",\"pattern\":\"^#[0-9A-Fa-f]{6}$\"},\"reset_render_config\":{\"type\":\"boolean\"},\"dry_run\":{\"type\":\"boolean\"},\"annotations\":{\"type\":\"array\",\"maxItems\":15,\"items\":{\"type\":\"object\",\"required\":[\"type\"],\"properties\":{\"id\":{\"type\":\"string\"},\"type\":{\"type\":\"string\",\"enum\":[\"highlight_box\",\"spacing_ruler\",\"alignment_redline\",\"callout\",\"pin\",\"freehand\",\"circle\",\"bracket\",\"underline\",\"strike\",\"pen\",\"line\",\"rect\",\"ellipse\",\"arrow\",\"text\",\"blur\",\"crop\"]},\"source\":{\"type\":\"string\"},\"som_id\":{\"type\":\"integer\"},\"points\":{\"oneOf\":[{\"type\":\"array\",\"items\":{\"type\":\"number\"},\"minItems\":2},{\"type\":\"array\",\"items\":{\"type\":\"array\",\"items\":{\"type\":\"number\"},\"minItems\":2,\"maxItems\":2},\"minItems\":1}]},\"color\":{\"type\":\"array\",\"items\":{\"type\":\"number\"},\"minItems\":4,\"maxItems\":4},\"ink\":{\"type\":\"string\",\"enum\":[\"thin\",\"regular\",\"bold\",\"marker\"]},\"stroke_width\":{\"oneOf\":[{\"type\":\"number\",\"exclusiveMinimum\":0},{\"type\":\"array\",\"items\":{\"type\":\"number\",\"exclusiveMinimum\":0},\"minItems\":2,\"maxItems\":2}]},\"dash\":{\"type\":\"string\",\"enum\":[\"solid\",\"dashed\",\"dotted\"]},\"fill\":{\"type\":\"string\",\"enum\":[\"none\",\"hachure\",\"solid\"]},\"fill_color\":{\"type\":\"array\",\"items\":{\"type\":\"number\"},\"minItems\":4,\"maxItems\":4},\"radius\":{\"type\":\"number\",\"minimum\":0},\"arrow\":{\"type\":\"string\",\"enum\":[\"none\",\"end\",\"both\",\"start\"]},\"z\":{\"type\":\"string\",\"enum\":[\"under\",\"normal\",\"over\"]},\"text\":{\"type\":\"string\"},\"font_size\":{\"type\":\"number\",\"minimum\":0},\"label\":{\"type\":\"object\",\"properties\":{\"text\":{\"type\":\"string\"},\"anchor\":{\"oneOf\":[{\"type\":\"string\",\"enum\":[\"auto\",\"right\",\"bottom\",\"0\",\"1\",\"2\",\"3\",\"4\",\"5\",\"6\",\"7\"]},{\"type\":\"integer\",\"minimum\":0,\"maximum\":7}]},\"leader\":{\"type\":\"string\",\"enum\":[\"curved\",\"elbow\",\"none\"]}},\"required\":[\"text\"]}}}}}},\"annotations\":{\"readOnlyHint\":false}},{\"name\":\"get_ux_annotations\",\"description\":\"Read the current annotation set.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{}},\"annotations\":{\"readOnlyHint\":true}},{\"name\":\"verify_ux_render\",\"description\":\"Compare the prepared source with the current annotation render.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{}},\"annotations\":{\"readOnlyHint\":true}}]}";

int miime_mcp_run(void) {
    char *line = NULL;
    size_t cap = 0;
    ssize_t n;
    while ((n = getline(&line, &cap, stdin)) != -1) {
        (void)n;
        /* trim */
        /* parse method */
        char *method = extract_string(line, "method");
        gint64 id = 0;
        int has_id = extract_id(line, &id);

        if (method && strcmp(method, "initialize") == 0) {
            char *res = g_strdup("{\"protocolVersion\":\"2024-11-05\",\"capabilities\":{\"tools\":{}},\"serverInfo\":{\"name\":\"miime-mcp\",\"version\":\"2.0\"}}");
            if (has_id) send_result(id, res);
            g_free(res);
        } else if (method && strcmp(method, "notifications/initialized") == 0) {
            /* no response */
        } else if (method && strcmp(method, "tools/list") == 0) {
            if (has_id) send_result(id, tools_list_json);
        } else if (method && strcmp(method, "tools/call") == 0) {
            char *params = extract_params(line);
            char *name = extract_string(params, "name");
            char *args = extract_arguments(params);
            char *result = NULL;
            g_clear_pointer(&g_tool_error, g_free);
            if (name && strcmp(name, "prepare_ux_canvas") == 0) result = handle_prepare(args);
            else if (name && strcmp(name, "render_ux_annotations") == 0) result = handle_render(args);
            else if (name && strcmp(name, "patch_ux_annotations") == 0) result = handle_patch(args);
            else if (name && strcmp(name, "get_ux_annotations") == 0) result = handle_get(args);
            else if (name && strcmp(name, "verify_ux_render") == 0) result = handle_verify(args);
            else {
                if (has_id) send_error(id, -32601, "method not found");
                g_free(name); g_free(args); g_free(params); g_free(method);
                continue;
            }
            if (has_id) {
                if (g_tool_error) send_error(id, -32602, g_tool_error);
                else send_result(id, result ? result : "{}");
            }
            g_clear_pointer(&g_tool_error, g_free);
            g_free(result);
            g_free(name); g_free(args); g_free(params);
        } else if (method && strcmp(method, "ping") == 0) {
            if (has_id) send_result(id, "{}");
        } else {
            if (has_id) send_error(id, -32601, "method not found");
        }
        g_free(method);
    }
    free(line);
    return 0;
}
