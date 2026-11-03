#ifndef MIIME_GROUNDING_H
#define MIIME_GROUNDING_H

#include <glib.h>
#include <cairo.h>

typedef struct {
    int width;
    int height;
} MiimeViewport;

typedef struct {
    int id;
    char tag[64];
    int x, y, w, h;
    int category;     /* 0 interactive, 1 typography, 2 container */
    double contrast;  /* WCAG contrast ratio vs background */
    char telemetry[256];
} MiimeGroundingElement;

typedef struct {
    MiimeGroundingElement *items;
    int count;
    int capacity;
} MiimeGroundingRegistry;

/* Run SoM grounding. source_type: 0=url, 1=image_path. Returns 0 on success. */
int miime_grounding_run(int source_type, const char *source, MiimeViewport viewport,
                        int visual_aids, char **out_buffer_b_path, char **out_registry_json);

void miime_grounding_registry_free(MiimeGroundingRegistry *reg);
char *miime_grounding_image_hash(const unsigned char *data, int len);

#endif /* MIIME_GROUNDING_H */
