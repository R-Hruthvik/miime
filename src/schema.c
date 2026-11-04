#include "schema.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <limits.h>

static GQuark schema_error_quark(void) { return g_quark_from_static_string("miime-schema-error"); }

/* ---------- Minimal JSON parser ---------- */

typedef enum { J_NULL, J_BOOL, J_NUM, J_STR, J_ARR, J_OBJ } JType;
typedef struct JVal JVal;
struct JVal {
    JType type;
    char *str;          /* for STR */
    double num;         /* for NUM */
    int boolean;        /* for BOOL */
    JVal **items;       /* for ARR/OBJ values */
    char **keys;        /* for OBJ keys */
    int count;          /* arr/obj count */
    int cap;
};

static void jfree(JVal *v) {
    if (!v) return;
    if (v->str) free(v->str);
    for (int i = 0; i < v->count; i++) {
        if (v->keys) free(v->keys[i]);
        jfree(v->items[i]);
    }
    free(v->items);
    free(v->keys);
    free(v);
}

static JVal *jnew(JType t) {
    JVal *v = calloc(1, sizeof(JVal));
    if (v) v->type = t;
    return v;
}

static void skip_ws(const char **p) {
    while (**p && isspace((unsigned char)**p)) (*p)++;
}

static int json_hex4(const char *p, gunichar *out) {
    gunichar value = 0;
    for (int i = 0; i < 4; i++) {
        unsigned char c = (unsigned char)p[i];
        int digit = c >= '0' && c <= '9' ? c - '0' :
                    c >= 'a' && c <= 'f' ? c - 'a' + 10 :
                    c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        if (digit < 0) return 0;
        value = (value << 4) | (gunichar)digit;
    }
    *out = value;
    return 1;
}

static void bounded_utf8_copy(char *dst, size_t capacity, const char *src) {
    if (!capacity) return;
    size_t used = 0;
    const char *p = src ? src : "";
    while (*p) {
        const char *next = g_utf8_next_char(p);
        size_t bytes = (size_t)(next - p);
        if (used + bytes >= capacity) break;
        memcpy(dst + used, p, bytes);
        used += bytes;
        p = next;
    }
    dst[used] = '\0';
}

static JVal *parse_value(const char **p);

static char *parse_string_raw(const char **p) {
    if (**p != '"') return NULL;
    (*p)++;
    size_t cap = 64, len = 0;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    while (**p && **p != '"') {
        if ((unsigned char)**p < 0x20) { free(buf); return NULL; }
        if (**p == '\\') {
            (*p)++;
            if (!**p) { free(buf); return NULL; }
            char c = **p;
            if (c == 'u') {
                gunichar ch;
                if (strlen(*p + 1) < 4 || !json_hex4(*p + 1, &ch)) { free(buf); return NULL; }
                *p += 5;
                if (ch >= 0xD800 && ch <= 0xDBFF) {
                    if ((*p)[0] != '\\' || (*p)[1] != 'u' || strlen(*p + 2) < 4) { free(buf); return NULL; }
                    gunichar low;
                    if (!json_hex4(*p + 2, &low) || low < 0xDC00 || low > 0xDFFF) { free(buf); return NULL; }
                    ch = 0x10000 + ((ch - 0xD800) << 10) + (low - 0xDC00);
                    *p += 6;
                } else if (ch >= 0xDC00 && ch <= 0xDFFF) { free(buf); return NULL; }
                char encoded[6];
                int n = g_unichar_to_utf8(ch, encoded);
                if (len + (size_t)n + 1 >= cap) { while (len + (size_t)n + 1 >= cap) cap *= 2; char *next = realloc(buf, cap); if (!next) { free(buf); return NULL; } buf = next; }
                memcpy(buf + len, encoded, (size_t)n); len += (size_t)n;
                continue;
            }
            char out = c;
            switch (c) {
                case 'n': out = '\n'; break;
                case 't': out = '\t'; break;
                case 'r': out = '\r'; break;
                case '"': out = '"'; break;
                case '\\': out = '\\'; break;
                case '/': out = '/'; break;
                default: out = c; break;
            }
            if (len + 2 >= cap) { cap *= 2; char *next = realloc(buf, cap); if (!next) { free(buf); return NULL; } buf = next; }
            buf[len++] = out;
            (*p)++;
        } else {
            if (len + 2 >= cap) { cap *= 2; char *next = realloc(buf, cap); if (!next) { free(buf); return NULL; } buf = next; }
            buf[len++] = **p;
            (*p)++;
        }
    }
    if (**p != '"') { free(buf); return NULL; }
    (*p)++;
    buf[len] = '\0';
    return buf;
}

static JVal *parse_string(const char **p) {
    char *s = parse_string_raw(p);
    if (!s) return NULL;
    JVal *v = jnew(J_STR);
    v->str = s;
    return v;
}

static JVal *parse_number(const char **p) {
    char *end;
    double d = strtod(*p, &end);
    if (end == *p) return NULL;
    JVal *v = jnew(J_NUM);
    v->num = d;
    *p = end;
    return v;
}

static JVal *parse_array(const char **p) {
    if (**p != '[') return NULL;
    (*p)++;
    skip_ws(p);
    JVal *v = jnew(J_ARR);
    if (**p == ']') { (*p)++; return v; }
    gboolean closed = FALSE;
    while (**p) {
        skip_ws(p);
        JVal *item = parse_value(p);
        if (!item) { jfree(v); return NULL; }
        if (v->count >= v->cap) {
            int cap = v->cap ? v->cap * 2 : 8;
            JVal **items = realloc(v->items, sizeof(JVal*) * (size_t)cap);
            if (!items) { jfree(item); jfree(v); return NULL; }
            v->items = items; v->cap = cap;
        }
        v->items[v->count++] = item;
        skip_ws(p);
        if (**p == ',') { (*p)++; continue; }
        if (**p == ']') { (*p)++; closed = TRUE; break; }
        jfree(v); return NULL;
    }
    if (!closed) { jfree(v); return NULL; }
    return v;
}

static JVal *parse_object(const char **p) {
    if (**p != '{') return NULL;
    (*p)++;
    skip_ws(p);
    JVal *v = jnew(J_OBJ);
    if (**p == '}') { (*p)++; return v; }
    gboolean closed = FALSE;
    while (**p) {
        skip_ws(p);
        if (**p != '"') { jfree(v); return NULL; }
        char *key = parse_string_raw(p);
        if (!key) { jfree(v); return NULL; }
        skip_ws(p);
        if (**p != ':') { free(key); jfree(v); return NULL; }
        (*p)++;
        skip_ws(p);
        JVal *val = parse_value(p);
        if (!val) { free(key); jfree(v); return NULL; }
        if (v->count >= v->cap) {
            int cap = v->cap ? v->cap * 2 : 8;
            char **keys = realloc(v->keys, sizeof(char*) * (size_t)cap);
            if (!keys) { free(key); jfree(val); jfree(v); return NULL; }
            v->keys = keys;
            JVal **items = realloc(v->items, sizeof(JVal*) * (size_t)cap);
            if (!items) { free(key); jfree(val); jfree(v); return NULL; }
            v->items = items; v->cap = cap;
        }
        v->keys[v->count] = key;
        v->items[v->count] = val;
        v->count++;
        skip_ws(p);
        if (**p == ',') { (*p)++; continue; }
        if (**p == '}') { (*p)++; closed = TRUE; break; }
        jfree(v); return NULL;
    }
    if (!closed) { jfree(v); return NULL; }
    return v;
}

static JVal *parse_value(const char **p) {
    skip_ws(p);
    if (**p == '"') return parse_string(p);
    if (**p == '{') return parse_object(p);
    if (**p == '[') return parse_array(p);
    if (strncmp(*p, "true", 4) == 0) { JVal *v = jnew(J_BOOL); v->boolean = 1; *p += 4; return v; }
    if (strncmp(*p, "false", 5) == 0) { JVal *v = jnew(J_BOOL); v->boolean = 0; *p += 5; return v; }
    if (strncmp(*p, "null", 4) == 0) { JVal *v = jnew(J_NULL); *p += 4; return v; }
    return parse_number(p);
}

static JVal *jget(JVal *obj, const char *key) {
    if (!obj || obj->type != J_OBJ) return NULL;
    for (int i = 0; i < obj->count; i++) {
        if (strcmp(obj->keys[i], key) == 0) return obj->items[i];
    }
    return NULL;
}

static int enum_value(JVal *v, const char *const *names, int count, int fallback) {
    if (!v || v->type != J_STR) return fallback;
    for (int i = 0; i < count; i++) if (strcmp(v->str, names[i]) == 0) return i;
    return fallback;
}
static void append_json_string(GString *g, const char *s) {
    g_string_append_c(g, '"');
    for (const unsigned char *c = (const unsigned char *)s; *c; c++) {
        if (*c == '"' || *c == '\\') { g_string_append_c(g, '\\'); g_string_append_c(g, (char)*c); }
        else if (*c == '\n') g_string_append(g, "\\n");
        else if (*c == '\r') g_string_append(g, "\\r");
        else if (*c == '\t') g_string_append(g, "\\t");
        else g_string_append_c(g, (char)*c);
    }
    g_string_append_c(g, '"');
}
static int enum_valid(JVal *v, const char *const *names, int count) {
    return v && v->type == J_STR && enum_value(v, names, count, -1) >= 0;
}
static const char *enum_name(int value, const char *const *names, int count) {
    return value >= 0 && value < count ? names[value] : NULL;
}

static const char *type_to_str(MiimeAnnotationType t) {
    switch (t) {
        case MIIME_ANNOT_PEN: return "pen";
        case MIIME_ANNOT_LINE: return "line";
        case MIIME_ANNOT_RECT: return "rect";
        case MIIME_ANNOT_ELLIPSE: return "ellipse";
        case MIIME_ANNOT_ARROW: return "arrow";
        case MIIME_ANNOT_TEXT: return "text";
        case MIIME_ANNOT_BLUR: return "blur";
        case MIIME_ANNOT_CROP: return "crop";
        case MIIME_ANNOT_HIGHLIGHT_BOX: return "highlight_box";
        case MIIME_ANNOT_SPACING_RULER: return "spacing_ruler";
        case MIIME_ANNOT_ALIGNMENT_REDLINE: return "alignment_redline";
        case MIIME_ANNOT_CALLOUT: return "callout";
        case MIIME_ANNOT_PIN: return "pin";
        case MIIME_ANNOT_FREEHAND: return "freehand";
        case MIIME_ANNOT_CIRCLE: return "circle";
        case MIIME_ANNOT_BRACKET: return "bracket";
        case MIIME_ANNOT_UNDERLINE: return "underline";
        case MIIME_ANNOT_STRIKE: return "strike";
    }
    return "pen";
}

static MiimeAnnotationType str_to_type(const char *s) {
    if (!s) return MIIME_ANNOT_PEN;
    if (strcmp(s, "highlight_box") == 0) return MIIME_ANNOT_HIGHLIGHT_BOX;
    if (strcmp(s, "spacing_ruler") == 0) return MIIME_ANNOT_SPACING_RULER;
    if (strcmp(s, "alignment_redline") == 0) return MIIME_ANNOT_ALIGNMENT_REDLINE;
    if (strcmp(s, "callout") == 0) return MIIME_ANNOT_CALLOUT;
    if (strcmp(s, "pin") == 0) return MIIME_ANNOT_PIN;
    if (strcmp(s, "freehand") == 0) return MIIME_ANNOT_FREEHAND;
    if (strcmp(s, "circle") == 0) return MIIME_ANNOT_CIRCLE;
    if (strcmp(s, "bracket") == 0) return MIIME_ANNOT_BRACKET;
    if (strcmp(s, "underline") == 0) return MIIME_ANNOT_UNDERLINE;
    if (strcmp(s, "strike") == 0) return MIIME_ANNOT_STRIKE;
    if (strcmp(s, "line") == 0) return MIIME_ANNOT_LINE;
    if (strcmp(s, "rect") == 0) return MIIME_ANNOT_RECT;
    if (strcmp(s, "ellipse") == 0) return MIIME_ANNOT_ELLIPSE;
    if (strcmp(s, "arrow") == 0) return MIIME_ANNOT_ARROW;
    if (strcmp(s, "text") == 0) return MIIME_ANNOT_TEXT;
    if (strcmp(s, "blur") == 0) return MIIME_ANNOT_BLUR;
    if (strcmp(s, "crop") == 0) return MIIME_ANNOT_CROP;
    return MIIME_ANNOT_PEN;
}

static gboolean parse_complete_json(const char *json, JVal **out) {
    if (!json) return FALSE;
    const char *p = json;
    JVal *root = parse_value(&p);
    if (!root) return FALSE;
    skip_ws(&p);
    if (*p) { jfree(root); return FALSE; }
    *out = root;
    return TRUE;
}

gboolean miime_som_lookup_json(const char *registry_json, int som_id,
                               int *out_x, int *out_y, int *out_w, int *out_h) {
    JVal *root = NULL;
    if (!parse_complete_json(registry_json, &root) || root->type != J_ARR) {
        jfree(root);
        return FALSE;
    }
    gboolean found = FALSE;
    for (int i = 0; i < root->count; i++) {
        JVal *item = root->items[i];
        if (!item || item->type != J_OBJ) { jfree(root); return FALSE; }
        JVal *id = jget(item, "id");
        if (!id || id->type != J_NUM || !isfinite(id->num) || id->num < INT_MIN || id->num > INT_MAX || trunc(id->num) != id->num) {
            jfree(root); return FALSE;
        }
        if ((int)id->num != som_id) continue;
        JVal *bounds = jget(item, "bounds");
        if (!bounds || bounds->type != J_ARR || bounds->count != 4) { jfree(root); return FALSE; }
        int values[4];
        for (int k = 0; k < 4; k++) {
            JVal *v = bounds->items[k];
            if (!v || v->type != J_NUM || !isfinite(v->num) || v->num < INT_MIN || v->num > INT_MAX || trunc(v->num) != v->num) {
                jfree(root); return FALSE;
            }
            values[k] = (int)v->num;
        }
        if (values[2] <= 0 || values[3] <= 0) { jfree(root); return FALSE; }
        if (out_x) *out_x = values[0];
        if (out_y) *out_y = values[1];
        if (out_w) *out_w = values[2];
        if (out_h) *out_h = values[3];
        found = TRUE;
        break;
    }
    jfree(root);
    return found;
}

static void set_schema_error(GError **error, int code, const char *message) {
    if (error && !*error) *error = g_error_new_literal(schema_error_quark(), code, message);
}

static const char *const drawing_styles[] = {"technical","sketch","editorial","minimal"};
static const char *const themes[] = {"auto","light","dark","warm","neutral"};
static const char *const roles[] = {"none","error","warning","info"};

static gboolean parse_config_value(JVal *root, MiimeStyleConfig *out, GError **error) {
    if (!root || root->type != J_OBJ) { set_schema_error(error, 20, "render config must be a JSON object"); return FALSE; }
    MiimeStyleConfig tmp = {0};
    JVal *v = jget(root, "drawing_style");
    if (v) { if (!enum_valid(v,drawing_styles,4)) goto invalid; tmp.drawing_style=enum_value(v,drawing_styles,4,0); tmp.has_drawing_style=TRUE; }
    v = jget(root, "theme");
    if (v) { if (!enum_valid(v,themes,5)) goto invalid; tmp.theme=enum_value(v,themes,5,0); tmp.has_theme=TRUE; }
    v = jget(root, "analysis_background");
    if (v) {
        if (v->type != J_STR || strlen(v->str) != 7 || v->str[0] != '#') goto invalid;
        guint32 rgb=0;
        for (int i=1;i<7;i++) { int d=g_ascii_xdigit_value(v->str[i]); if (d<0) goto invalid; rgb=(rgb<<4)|(guint32)d; }
        tmp.analysis_background_rgba=(rgb<<8)|0xff; tmp.has_analysis_background=TRUE;
    }
    v = jget(root, "aesthetic_style");
    if (v) {
        static const char *const aesthetics[]={"rough_sketch","clean_technical"};
        if (!enum_valid(v,aesthetics,2)) goto invalid;
        tmp.aesthetic_style=enum_value(v,aesthetics,2,0); tmp.has_aesthetic_style=TRUE;
    }
    v = jget(root, "reset_render_config");
    if (v) { if (v->type != J_BOOL) goto invalid; tmp.has_reset_render_config=TRUE; tmp.reset_render_config=v->boolean; }
    *out=tmp;
    return TRUE;
invalid:
    set_schema_error(error, 21, "invalid render config field");
    return FALSE;
}

gboolean miime_schema_deserialize_render_config(const char *json, MiimeStyleConfig *out, GError **error) {
    if (!out) { set_schema_error(error,20,"output config is required"); return FALSE; }
    JVal *root=NULL;
    if (!parse_complete_json(json,&root)) { set_schema_error(error,2,"invalid render config JSON"); return FALSE; }
    MiimeStyleConfig tmp={0};
    gboolean ok=parse_config_value(root,&tmp,error);
    jfree(root);
    if (ok) *out=tmp;
    return ok;
}

char *miime_schema_serialize_render_config(const MiimeStyleConfig *config) {
    GString *g=g_string_new("{"); gboolean comma=FALSE;
    if (config) {
        if (config->has_drawing_style && enum_name(config->drawing_style,drawing_styles,4)) { g_string_append_printf(g,"\"drawing_style\":\"%s\"",enum_name(config->drawing_style,drawing_styles,4)); comma=TRUE; }
        if (config->has_theme && enum_name(config->theme,themes,5)) { if(comma)g_string_append_c(g,','); g_string_append_printf(g,"\"theme\":\"%s\"",enum_name(config->theme,themes,5)); comma=TRUE; }
        if (config->has_analysis_background) { if(comma)g_string_append_c(g,','); g_string_append_printf(g,"\"analysis_background\":\"#%06X\"",(unsigned)(config->analysis_background_rgba>>8)&0xffffff); comma=TRUE; }
        if (config->has_aesthetic_style && config->aesthetic_style>=MIIME_AESTHETIC_ROUGH_SKETCH && config->aesthetic_style<=MIIME_AESTHETIC_CLEAN_TECHNICAL) {
            if(comma)g_string_append_c(g,',');
            g_string_append_printf(g,"\"aesthetic_style\":\"%s\"",config->aesthetic_style==MIIME_AESTHETIC_ROUGH_SKETCH?"rough_sketch":"clean_technical");
        }
    }
    g_string_append_c(g,'}'); return g_string_free(g,FALSE);
}

static gboolean annotation_needs_target_points(const MiimeAnnotation *a) {
    return a->point_count == 0 ||
        (a->point_count == 2 && a->points[0] == 0.0 && a->points[1] == 0.0);
}

static gboolean expand_target_points(MiimeAnnotation *a, int x, int y, int w, int h,
                                     gboolean has_endpoint) {
    double right = (double)x + w, bottom = (double)y + h;
    switch (a->type) {
        case MIIME_ANNOT_HIGHLIGHT_BOX:
        case MIIME_ANNOT_RECT:
        case MIIME_ANNOT_CALLOUT:
        case MIIME_ANNOT_CIRCLE:
        case MIIME_ANNOT_ELLIPSE:
            a->points[0] = x; a->points[1] = y;
            a->points[2] = right; a->points[3] = y;
            a->points[4] = right; a->points[5] = bottom;
            a->points[6] = x; a->points[7] = bottom;
            a->point_count = 8;
            return TRUE;
        case MIIME_ANNOT_ARROW:
        case MIIME_ANNOT_LINE:
        case MIIME_ANNOT_UNDERLINE:
        case MIIME_ANNOT_STRIKE:
        case MIIME_ANNOT_FREEHAND:
            if (!has_endpoint) return FALSE;
            {
                int endpoint = a->point_count >= 4 ? 2 : 0;
                double ex = a->points[endpoint], ey = a->points[endpoint + 1];
                double cx = x + w / 2.0, cy = y + h / 2.0;
                double dx = ex - cx, dy = ey - cy;
                if (dx == 0.0 && dy == 0.0) return FALSE;
                double tx = dx == 0.0 ? INFINITY : (w / 2.0) / fabs(dx);
                double ty = dy == 0.0 ? INFINITY : (h / 2.0) / fabs(dy);
                double t = tx < ty ? tx : ty;
                a->points[0] = cx + dx * t;
                a->points[1] = cy + dy * t;
                a->points[2] = ex; a->points[3] = ey;
                a->point_count = 4;
            }
            return TRUE;
        case MIIME_ANNOT_PIN:
            a->points[0] = x + w / 2.0;
            a->points[1] = y + h / 2.0;
            a->point_count = 2;
            return TRUE;
        case MIIME_ANNOT_SPACING_RULER:
            a->points[0] = x; a->points[1] = y;
            a->points[2] = right; a->points[3] = y;
            a->point_count = 4;
            return TRUE;
        default:
            return FALSE;
    }
}

MiimeAnnotationStack *miime_schema_deserialize_ex(const char *json, GError **error,
                                                   const char *registry_json) {
    if (!json) {
        if (error) *error = g_error_new_literal(schema_error_quark(), 1, "null json");
        return NULL;
    }
    JVal *root = NULL;
    if (!parse_complete_json(json, &root) || root->type != J_OBJ) {
        if (root) jfree(root);
        if (error) *error = g_error_new_literal(schema_error_quark(), 2, "invalid json root");
        return NULL;
    }

    MiimeAnnotationStack *stack = miime_annot_stack_new();
    if (!stack) { jfree(root); return NULL; }

    JVal *anns = jget(root, "annotations");
    if (anns && anns->type == J_ARR) {
        if (anns->count > 15) {
            miime_annot_stack_free(stack);
            jfree(root);
            if (error) *error = g_error_new_literal(schema_error_quark(), 3, "maxItems 15 exceeded");
            return NULL;
        }
        for (int i = 0; i < anns->count; i++) {
            JVal *o = anns->items[i];
            if (!o || o->type != J_OBJ) continue;
            MiimeAnnotation a;
            memset(&a, 0, sizeof(a));
            a.som_id = -1; a.label_anchor = -1;
            JVal *id = jget(o, "id");
            if (id && id->type == J_STR) {
                bounded_utf8_copy(a.json_id, sizeof(a.json_id), id->str);
            }
            JVal *src = jget(o, "source");
            if (src && src->type == J_STR && strcmp(src->str, "agent") == 0)
                a.source = MIIME_SOURCE_AGENT;
            else
                a.source = MIIME_SOURCE_HUMAN;
            JVal *typ = jget(o, "type");
            if (typ && typ->type == J_STR) a.type = str_to_type(typ->str);
            JVal *pts = jget(o, "points");
            if (pts && pts->type == J_ARR) {
                /* Accept both flat [x0,y0,x1,y1,...] and nested [[x0,y0],[x1,y1],...]
                 * point lists. point_count = number of doubles stored (pairs * 2),
                 * which is what render.c requires (>= 4). */
                int count = 0;
                for (int k = 0; k < pts->count && count < MIIME_MAX_POINTS; k++) {
                    JVal *it = pts->items[k];
                    if (!it) continue;
                    if (it->type == J_NUM) {
                        a.points[count++] = it->num;
                    } else if (it->type == J_ARR) {
                        for (int j = 0; j < it->count && count < MIIME_MAX_POINTS; j++) {
                            if (it->items[j] && it->items[j]->type == J_NUM)
                                a.points[count++] = it->items[j]->num;
                        }
                    }
                }
                a.point_count = count;
            }
            JVal *col = jget(o, "color");
            if (col && col->type == J_ARR && col->count >= 4) {
                a.has_color=TRUE;
                if (col->items[0]->type == J_NUM) a.color_r = col->items[0]->num;
                if (col->items[1]->type == J_NUM) a.color_g = col->items[1]->num;
                if (col->items[2]->type == J_NUM) a.color_b = col->items[2]->num;
                if (col->items[3]->type == J_NUM) a.color_a = col->items[3]->num;
            } else {
                a.color_r = 1.0; a.color_g = 0.0; a.color_b = 0.0; a.color_a = 1.0;
            }
            static const char *const inks[] = {"thin", "regular", "bold", "marker"};
            static const char *const fills[] = {"none", "hachure", "solid"};
            static const char *const dashes[] = {"solid", "dashed", "dotted"};
            static const char *const arrows[] = {"none", "start", "end", "both"};
            static const char *const zs[] = {"under", "normal", "over"};
            JVal *v = jget(o, "ink"); if (enum_valid(v, inks, 4)) { a.ink=enum_value(v, inks, 4, 1); a.has_ink=TRUE; }
            JVal *sw = jget(o, "stroke_width");
            if (sw) {
                if (sw->type == J_NUM && isfinite(sw->num) && sw->num > 0.0) {
                    a.stroke_width=sw->num; a.has_stroke_width=TRUE;
                } else if (sw->type == J_ARR && sw->count == 2 && sw->items[0]->type == J_NUM && sw->items[1]->type == J_NUM &&
                           isfinite(sw->items[0]->num) && isfinite(sw->items[1]->num) && sw->items[0]->num > 0.0 && sw->items[1]->num > 0.0) {
                    a.stroke_width=sw->items[0]->num; a.stroke_width_end=sw->items[1]->num;
                    a.has_stroke_width=TRUE; a.has_stroke_width_pair=TRUE;
                } else {
                    miime_annot_stack_free(stack); jfree(root);
                    if (error) *error=g_error_new_literal(schema_error_quark(),4,"stroke_width must be a positive finite number or a pair of positive finite numbers");
                    return NULL;
                }
            } else if (a.has_ink) {
                a.stroke_width=(double[]){1.5,2.5,4.0,14.0}[a.ink];
            } else a.stroke_width=2.0;
            v = jget(o, "fill"); if (enum_valid(v, fills, 3)) { a.fill=enum_value(v, fills, 3, 0); a.has_fill=TRUE; }
            v = jget(o, "dash"); if (enum_valid(v, dashes, 3)) { a.dash=enum_value(v, dashes, 3, 0); a.has_dash=TRUE; }
            v = jget(o, "arrow"); if (enum_valid(v, arrows, 4)) { a.arrow=enum_value(v, arrows, 4, 0); a.has_arrow=TRUE; }
            v = jget(o, "z"); if (enum_valid(v, zs, 3)) { a.z=enum_value(v, zs, 3, 1); a.has_z=TRUE; }
            v = jget(o, "som_id"); if (v) {
                if (v->type != J_NUM || !isfinite(v->num) || v->num < INT_MIN || v->num > INT_MAX || trunc(v->num) != v->num) {
                    miime_annot_stack_free(stack); jfree(root);
                    set_schema_error(error, 5, "som_id must be an integer");
                    return NULL;
                }
                a.som_id=(int)v->num; a.has_som_id=TRUE;
            }
            if (a.has_som_id) {
                const char *type_name = typ && typ->type == J_STR ? typ->str : NULL;
                static const char *const targetable_types[] = {
                    "highlight_box", "rect", "circle", "ellipse", "callout",
                    "arrow", "line", "underline", "strike", "freehand", "pin", "spacing_ruler"
                };
                gboolean known = FALSE;
                for (size_t ti = 0; ti < sizeof(targetable_types)/sizeof(targetable_types[0]); ti++)
                    if (type_name && strcmp(type_name, targetable_types[ti]) == 0) known = TRUE;
                int tx, ty, tw, th;
                if (!known || !registry_json || !miime_som_lookup_json(registry_json, a.som_id, &tx, &ty, &tw, &th)) {
                    miime_annot_stack_free(stack); jfree(root);
                    set_schema_error(error, 5, "unknown som_id (or missing bounds)");
                    return NULL;
                }
                gboolean has_endpoint = a.point_count >= 2;
                if (annotation_needs_target_points(&a) || a.type == MIIME_ANNOT_LINE ||
                    a.type == MIIME_ANNOT_ARROW || a.type == MIIME_ANNOT_UNDERLINE ||
                    a.type == MIIME_ANNOT_STRIKE || a.type == MIIME_ANNOT_FREEHAND) {
                    if (!expand_target_points(&a, tx, ty, tw, th, has_endpoint)) {
                        miime_annot_stack_free(stack); jfree(root);
                        set_schema_error(error, 6, "arrow/line require a second point");
                        return NULL;
                    }
                }
            }
            v = jget(o, "radius"); if (v && v->type == J_NUM) { a.radius=v->num; a.has_radius=TRUE; }
            v = jget(o, "fill_color"); if (v && v->type == J_ARR && v->count >= 4 && v->items[0]->type == J_NUM && v->items[1]->type == J_NUM && v->items[2]->type == J_NUM && v->items[3]->type == J_NUM) {
                a.fill_r=v->items[0]->num; a.fill_g=v->items[1]->num; a.fill_b=v->items[2]->num; a.fill_a=v->items[3]->num; a.has_fill_color=TRUE;
            }
            v = jget(o, "label"); if (v && v->type == J_OBJ) {
                JVal *lv = jget(v, "text"); if (lv && lv->type == J_STR) { bounded_utf8_copy(a.label_text,sizeof(a.label_text),lv->str); a.has_label=TRUE; }
                static const char *const leaders[] = {"curved", "elbow", "none"};
                lv=jget(v,"leader"); if (enum_valid(lv,leaders,3)) { a.label_leader=enum_value(lv,leaders,3,0); a.has_label_leader=TRUE; }
                lv=jget(v,"anchor"); if (lv && lv->type == J_NUM) {
                    if (isfinite(lv->num) && lv->num >= 0 && lv->num <= 7 && floor(lv->num) == lv->num) {
                        a.label_anchor=(int)lv->num;
                        a.has_label_anchor=TRUE;
                    }
                } else if (lv && lv->type == J_STR) {
                    gboolean valid=TRUE;
                    if (strcmp(lv->str,"auto")==0) a.label_anchor=-1;
                    else if (strcmp(lv->str,"right")==0) a.label_anchor=1;
                    else if (strcmp(lv->str,"bottom")==0) a.label_anchor=2;
                    else { char *end=NULL; long n=strtol(lv->str,&end,10); if (end && end!=lv->str && *end=='\0' && n>=0 && n<=7) a.label_anchor=(int)n; else valid=FALSE; }
                    if (valid) a.has_label_anchor=TRUE;
                }
            }
            JVal *txt = jget(o, "text");
            if (txt && txt->type == J_STR) bounded_utf8_copy(a.text, sizeof(a.text), txt->str);
            JVal *fs = jget(o, "font_size");
            if (fs && fs->type == J_NUM) { a.font_size = fs->num; a.has_font_size=TRUE; }
            v=jget(o,"role"); if (v) {
                if (!enum_valid(v,roles,4)) { miime_annot_stack_free(stack); jfree(root); set_schema_error(error,22,"role must be none, error, warning, or info"); return NULL; }
                a.role=enum_value(v,roles,4,0); a.has_role=TRUE;
            }
            v=jget(o,"label_surface"); if (v) {
                gboolean valid=v->type==J_ARR && v->count==4;
                for(int k=0;valid && k<4;k++) valid=v->items[k]->type==J_NUM && isfinite(v->items[k]->num) && v->items[k]->num>=0 && v->items[k]->num<=1;
                if (!valid) { miime_annot_stack_free(stack); jfree(root); set_schema_error(error,23,"label_surface must be four finite RGBA channels in [0,1]"); return NULL; }
                for(int k=0;k<4;k++) a.label_surface_rgba[k]=v->items[k]->num;
                a.has_label_surface=TRUE;
            }
            miime_annot_stack_push(stack, a);
        }
    }

    jfree(root);
    return stack;
}

MiimeAnnotationStack *miime_schema_deserialize(const char *json, GError **error) {
    return miime_schema_deserialize_ex(json, error, NULL);
}

gboolean miime_schema_validate(const char *json, GError **error) {
    MiimeAnnotationStack *s = miime_schema_deserialize(json, error);
    if (!s) return FALSE;
    miime_annot_stack_free(s);
    return TRUE;
}

static char *serialize_annotations(const MiimeAnnotationStack *stack, const MiimeStyleConfig *config) {
    static const char *const inks[] = {"thin","regular","bold","marker"};
    static const char *const dashes[] = {"solid","dashed","dotted"};
    static const char *const fills[] = {"none","hachure","solid"};
    static const char *const arrows[] = {"none","start","end","both"};
    static const char *const zs[] = {"under","normal","over"};
    static const char *const leaders[] = {"curved","elbow","none"};
    gboolean themed=config && (config->has_drawing_style || config->has_theme);
    GString *g = g_string_new("{");
    if(config) {
        char *cfg=miime_schema_serialize_render_config(config);
        if(cfg && strcmp(cfg,"{}")!=0) { g_string_append_len(g,cfg+1,strlen(cfg)-2); g_string_append_c(g,','); }
        g_free(cfg);
    }
    g_string_append(g,"\"annotations\":[");
    if (stack) {
        int count = miime_annot_stack_count(stack);
        for (int i = 0; i < count; i++) {
            const MiimeAnnotation *a = miime_annot_stack_get(stack, i);
            if (!a) continue;
            if (i > 0) g_string_append(g, ",");
            g_string_append(g, "{\"id\":"); append_json_string(g, a->json_id);
            g_string_append_printf(g, ",\"type\":\"%s\",\"source\":\"%s\",\"points\":[",
                                  type_to_str(a->type), a->source == MIIME_SOURCE_AGENT ? "agent" : "human");
            for (int k = 0; k < a->point_count; k++) {
                if (k > 0) g_string_append_c(g, ',');
                g_string_append_printf(g, "%.6g", a->points[k]);
            }
            g_string_append_c(g,']');
            if(!themed || a->has_color) g_string_append_printf(g, ",\"color\":[%.6g,%.6g,%.6g,%.6g]",a->color_r,a->color_g,a->color_b,a->color_a);
            if(!themed || a->has_stroke_width) {
                g_string_append(g,",\"stroke_width\":");
                if (a->has_stroke_width_pair) g_string_append_printf(g,"[%.6g,%.6g]",a->stroke_width,a->stroke_width_end);
                else g_string_append_printf(g,"%.6g",a->stroke_width);
            }
            g_string_append(g, ",\"text\":"); append_json_string(g, a->text);
            if(!themed || a->has_font_size) g_string_append_printf(g, ",\"font_size\":%.6g", a->font_size);
            if(a->has_role && enum_name(a->role,roles,4)) g_string_append_printf(g, ",\"role\":\"%s\"",enum_name(a->role,roles,4));
            if(a->has_label_surface) g_string_append_printf(g, ",\"label_surface\":[%.6g,%.6g,%.6g,%.6g]",a->label_surface_rgba[0],a->label_surface_rgba[1],a->label_surface_rgba[2],a->label_surface_rgba[3]);
            if (a->has_ink && enum_name(a->ink,inks,4)) g_string_append_printf(g, ",\"ink\":\"%s\"", enum_name(a->ink,inks,4));
            if (a->has_dash && enum_name(a->dash,dashes,3)) g_string_append_printf(g, ",\"dash\":\"%s\"", enum_name(a->dash,dashes,3));
            if (a->has_fill && enum_name(a->fill,fills,3)) g_string_append_printf(g, ",\"fill\":\"%s\"", enum_name(a->fill,fills,3));
            if (a->has_fill_color) g_string_append_printf(g, ",\"fill_color\":[%.6g,%.6g,%.6g,%.6g]",a->fill_r,a->fill_g,a->fill_b,a->fill_a);
            if (a->has_radius) g_string_append_printf(g, ",\"radius\":%.6g",a->radius);
            if (a->has_arrow && enum_name(a->arrow,arrows,4)) g_string_append_printf(g, ",\"arrow\":\"%s\"",enum_name(a->arrow,arrows,4));
            if (a->has_z && enum_name(a->z,zs,3)) g_string_append_printf(g, ",\"z\":\"%s\"",enum_name(a->z,zs,3));
            if (a->has_som_id) g_string_append_printf(g, ",\"som_id\":%d",a->som_id);
            gboolean emit_label_leader = a->has_label_leader && enum_name(a->label_leader,leaders,3);
            if (a->has_label || a->has_label_anchor || emit_label_leader) {
                gboolean comma=FALSE; g_string_append(g, ",\"label\":{");
                if (a->has_label) { g_string_append(g,"\"text\":"); append_json_string(g,a->label_text); comma=TRUE; }
                if (a->has_label_anchor && (a->label_anchor == -1 || (a->label_anchor >= 0 && a->label_anchor <= 7))) {
                    if (comma) g_string_append_c(g,',');
                    if (a->label_anchor==-1) g_string_append(g,"\"anchor\":\"auto\"");
                    else if (a->label_anchor==1) g_string_append(g,"\"anchor\":\"right\"");
                    else if (a->label_anchor==2) g_string_append(g,"\"anchor\":\"bottom\"");
                    else g_string_append_printf(g,"\"anchor\":\"%d\"",a->label_anchor);
                    comma=TRUE;
                }
                if (emit_label_leader) { if (comma) g_string_append_c(g,','); g_string_append_printf(g,"\"leader\":\"%s\"",enum_name(a->label_leader,leaders,3)); }
                g_string_append_c(g,'}');
            }
            g_string_append_c(g,'}');
        }
    }
    g_string_append(g, "]}");
    char *out = g_strdup(g->str);
    g_string_free(g, TRUE);
    return out;
}

char *miime_schema_serialize(const MiimeAnnotationStack *stack) { return serialize_annotations(stack,NULL); }
char *miime_schema_serialize_with_config(const MiimeAnnotationStack *stack, const MiimeStyleConfig *config) {
    if(!config || (!config->has_drawing_style && !config->has_theme && !config->has_analysis_background && !config->has_aesthetic_style)) return miime_schema_serialize(stack);
    return serialize_annotations(stack,config);
}
