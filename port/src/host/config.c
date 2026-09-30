#include "config.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#define APP "maniac-mansion-port"

typedef struct {
    char section[32], key[48], value[512];
} entry_t;

typedef struct {
    entry_t *e;
    int n, cap;
} table_t;

struct config {
    table_t file, cli; /* precedence: cli > environment > file */
    char *path;
};

static char *trim(char *s)
{
    while (isspace((unsigned char)*s))
        s++;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1]))
        *--e = 0;
    return s;
}

static void put(table_t *c, const char *sec, const char *key, const char *val)
{
    for (int i = 0; i < c->n; i++)
        if (!strcasecmp(c->e[i].section, sec) && !strcasecmp(c->e[i].key, key)) {
            snprintf(c->e[i].value, sizeof c->e[i].value, "%s", val);
            return;
        }
    if (c->n == c->cap) {
        c->cap = c->cap ? c->cap * 2 : 32;
        c->e = realloc(c->e, (size_t)c->cap * sizeof *c->e);
    }
    entry_t *e = &c->e[c->n++];
    snprintf(e->section, sizeof e->section, "%s", sec);
    snprintf(e->key, sizeof e->key, "%s", key);
    snprintf(e->value, sizeof e->value, "%s", val);
}

config_t *config_load(const char *path, char *err, int errlen)
{
    config_t *c = calloc(1, sizeof *c);
    char def[1024];
    bool explicit_path = path != NULL;
    if (!path) {
        snprintf(def, sizeof def, "%s/config.ini", path_config_dir());
        path = def;
    }
    c->path = strdup(path);
    FILE *f = fopen(path, "r");
    if (!f) {
        if (explicit_path) {
            snprintf(err, errlen, "cannot open config '%s': %s", path, strerror(errno));
            config_free(c);
            return NULL;
        }
        return c; /* no config file: defaults */
    }
    char line[1024], sec[32] = "";
    int ln = 0;
    while (fgets(line, sizeof line, f)) {
        ln++;
        char *h = strpbrk(line, "#;");
        if (h)
            *h = 0;
        char *s = trim(line);
        if (!*s)
            continue;
        if (*s == '[') {
            char *e = strchr(s, ']');
            if (e) {
                *e = 0;
                snprintf(sec, sizeof sec, "%s", trim(s + 1));
            }
            continue;
        }
        char *eq = strchr(s, '=');
        if (!eq) {
            fprintf(stderr, "warning: %s:%d: ignoring line without '='\n", path, ln);
            continue;
        }
        *eq = 0;
        put(&c->file, sec, trim(s), trim(eq + 1));
    }
    fclose(f);
    return c;
}

void config_set(config_t *c, const char *section_key, const char *value)
{
    char sec[32] = "", key[48];
    const char *dot = strchr(section_key, '.');
    if (dot) {
        snprintf(sec, sizeof sec, "%.*s", (int)(dot - section_key), section_key);
        snprintf(key, sizeof key, "%s", dot + 1);
    } else {
        snprintf(key, sizeof key, "%s", section_key);
    }
    put(&c->cli, sec, key, value);
}

static const char *lookup(const table_t *t, const char *section, const char *key)
{
    for (int i = t->n - 1; i >= 0; i--)
        if (!strcasecmp(t->e[i].section, section) && !strcasecmp(t->e[i].key, key))
            return t->e[i].value;
    return NULL;
}

const char *config_str(const config_t *c, const char *section, const char *key, const char *def)
{
    const char *v = lookup(&c->cli, section, key);
    if (v)
        return v;
    char env[128];
    snprintf(env, sizeof env, "MM_%s_%s", section, key);
    for (char *p = env; *p; p++)
        *p = (char)toupper((unsigned char)*p);
    if ((v = getenv(env)))
        return v;
    v = lookup(&c->file, section, key);
    return v ? v : def;
}

int config_int(const config_t *c, const char *section, const char *key, int def)
{
    const char *v = config_str(c, section, key, NULL);
    return v ? (int)strtol(v, NULL, 0) : def;
}

bool config_bool(const config_t *c, const char *section, const char *key, bool def)
{
    const char *v = config_str(c, section, key, NULL);
    if (!v)
        return def;
    return !strcasecmp(v, "on") || !strcasecmp(v, "true") || !strcasecmp(v, "yes") || !strcmp(v, "1");
}

void config_free(config_t *c)
{
    if (c) {
        free(c->path);
        free(c->file.e);
        free(c->cli.e);
    }
    free(c);
}

/* Save one setting without reserializing ROM paths, mappings or comments.
 * Use a same-directory atomic replacement; failure leaves the prior file intact. */
bool config_save_bool(config_t *c, const char *section, const char *key, bool value)
{
    if (!c || !c->path || !section || !key || strpbrk(section, "[]\r\n") ||
        strpbrk(key, "=\r\n"))
        return false;
    char *dir = strdup(c->path);
    if (!dir) return false;
    char *slash = strrchr(dir, '/');
    if (slash) {
        *slash = 0;
        if (*dir && !path_mkdirs(dir)) { free(dir); return false; }
    }
    free(dir);
    FILE *in = fopen(c->path, "r");
    if (!in && errno != ENOENT) return false;
    size_t len = strlen(c->path) + 16;
    char *tmp = malloc(len);
    if (!tmp) { if (in) fclose(in); return false; }
    snprintf(tmp, len, "%s.tmp.XXXXXX", c->path);
    int fd = mkstemp(tmp);
    if (fd < 0) { if (in) fclose(in); free(tmp); return false; }
    struct stat st;
    if (stat(c->path, &st) == 0) fchmod(fd, st.st_mode & 0777);
    FILE *out = fdopen(fd, "w");
    if (!out) { close(fd); unlink(tmp); free(tmp); if (in) fclose(in); return false; }
    char *line = NULL; size_t cap = 0;
    bool target = false, found = false, newline = true, ok = true;
    while (in && getline(&line, &cap, in) >= 0) {
        char *copy = strdup(line);
        if (!copy) { ok = false; break; }
        char *text = trim(copy);
        if (*text == '[') {
            char *end = strchr(text, ']');
            if (end) { *end = 0; target = !strcasecmp(trim(text + 1), section); }
        }
        char *eq = target && *text != '#' && *text != ';' ? strchr(text, '=') : NULL;
        bool replace = false;
        if (eq) { *eq = 0; replace = !strcasecmp(trim(text), key); }
        if (replace) {
            if (fprintf(out, "%s = %s\n", key, value ? "on" : "off") < 0) ok = false;
            found = true; newline = true;
        } else {
            if (fputs(line, out) == EOF) ok = false;
            size_t n = strlen(line); newline = n && line[n - 1] == '\n';
        }
        free(copy);
    }
    if (in) { if (ferror(in)) ok = false; fclose(in); }
    free(line);
    if (!found && fprintf(out, "%s[%s]\n%s = %s\n", newline ? "" : "\n",
                          section, key, value ? "on" : "off") < 0) ok = false;
    if (fflush(out) != 0 || fsync(fd) != 0) ok = false;
    if (fclose(out) != 0) ok = false;
    if (ok && rename(tmp, c->path) != 0) ok = false;
    if (!ok) unlink(tmp);
    free(tmp);
    if (ok) put(&c->file, section, key, value ? "on" : "off");
    return ok;
}

static const char *xdg(const char *var, const char *fallback, char *buf, size_t n)
{
    const char *v = getenv(var);
    if (v && *v)
        snprintf(buf, n, "%s/" APP, v);
    else
        snprintf(buf, n, "%s/%s/" APP, getenv("HOME") ? getenv("HOME") : ".", fallback);
    return buf;
}

const char *path_config_dir(void)
{
    static char b[1024];
    return xdg("XDG_CONFIG_HOME", ".config", b, sizeof b);
}

const char *path_data_dir(void)
{
    static char b[1024];
    return xdg("XDG_DATA_HOME", ".local/share", b, sizeof b);
}

const char *path_state_dir(void)
{
    static char b[1024];
    return xdg("XDG_STATE_HOME", ".local/state", b, sizeof b);
}

bool path_mkdirs(const char *dir)
{
    char p[1024];
    snprintf(p, sizeof p, "%s", dir);
    for (char *s = p + 1; *s; s++)
        if (*s == '/') {
            *s = 0;
            mkdir(p, 0755);
            *s = '/';
        }
    return mkdir(p, 0755) == 0 || errno == EEXIST;
}
