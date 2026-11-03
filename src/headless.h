#ifndef MIIME_HEADLESS_H
#define MIIME_HEADLESS_H

/* Run headless pipeline. Returns 0 on success. */
int miime_headless_run(const char *input_path, const char *annotations_path, const char *output_path);

#endif /* MIIME_HEADLESS_H */