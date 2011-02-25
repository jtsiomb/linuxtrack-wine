#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <alloca.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "cfg.h"
#include "logger.h"
#include "xmltree.h"

struct appdb_node {
	int id;
	char *name;
	struct appdb_node *left, *right;
};

static void init_defaults(void);
static int read_config(const char *fname, struct config *cfg);
static struct appdb_node *create_appdb(struct xml_node *xml);
static int appdb_add(struct appdb_node **treeptr, int id, const char *name);
static const char *appdb_find(struct appdb_node *tree, int id);
static void appdb_free(struct appdb_node *tree);
static int appdb_count_nodes(struct appdb_node *tree);
static char *strip_spaces(char *buf);
static void sanitize_appname(char *name);

static struct config cfg;


int init_config(void)
{
	char cfgdir[PATH_MAX];
	char path_buf[PATH_MAX];
	struct xml_node *xmltree;
	struct passwd *pwd;
	struct stat st;

	init_defaults();

	/* find config directory ~/.ltrdll */
	if(!(pwd = getpwuid(getuid()))) {
		logmsg("Failed to retreive user's home directory, and thus can't find the config files. Continuing with defaults\n");
		return -1;
	}
	sprintf(cfgdir, "%s/.ltrdll", pwd->pw_dir);

	if(stat(cfgdir, &st) == -1) {
		logmsg("Config dir (%s) doesn't exist. Will create it and continue with defaults\n", cfgdir);
		if(mkdir(cfgdir, 0775) == -1) {
			logmsg("Failed to create missing config file directory: %s: %s\n", cfgdir, strerror(errno));
			return -1;
		}
		if(stat(cfgdir, &st) == -1) {
			logmsg("WTF?\n");
			return -1;
		}
	}
	if(!S_ISDIR(st.st_mode)) {
		logmsg("%s exists but it's not a directory! can't read or store config files. Continuing with defaults\n", cfgdir);
		return -1;
	}

	/* read config file */
	sprintf(path_buf, "%s/config", cfgdir);
	read_config(path_buf, &cfg);

	/* create application database */
	sprintf(path_buf, "%s/apps.xml", cfgdir);

	if(!(xmltree = xml_read_tree(path_buf))) {
		logmsg("failed to read apps list: %s\n", path_buf);
	} else {
		cfg.appdb = create_appdb(xmltree);
		xml_free_tree(xmltree);
		logmsg("loaded %d games in the appdb\n", appdb_count_nodes(cfg.appdb));
	}

	return 0;
}

void cleanup_config(void)
{
	appdb_free(cfg.appdb);
}

struct config *get_config(void)
{
	return &cfg;
}

const char *app_name(int app_id)
{
	if(app_id < 0 || !cfg.appdb) {
		return 0;
	}
	return appdb_find(cfg.appdb, app_id);
}

int add_ltr_profile(const char *name)
{
	FILE *fp;
	struct passwd *pwd;
	char line[512];
	char *fname, *clean_name;

	if(!(pwd = getpwuid(getuid()))) {
		logmsg("failed to add linuxtrack profile, can't find the home directory: %s\n", strerror(errno));
		return -1;
	}
	fname = alloca(strlen("/.linuxtrack") + strlen(pwd->pw_dir) + 1);
	sprintf(fname, "%s/.linuxtrack", pwd->pw_dir);

	if(!(fp = fopen(fname, "rb+"))) {
		logmsg("failed to open linuxtrack config file: %s: %s\n", fname, strerror(errno));
		return -1;
	}

	while(fgets(line, sizeof line, fp)) {
		char *tok, *delim;

		if(!(delim = strchr(line, '='))) {
			continue;
		}
		*delim = 0;

		tok = strip_spaces(line);
		if(tok && strcasecmp(tok, "Title") == 0) {
			if(!(tok = strip_spaces(delim + 1))) {
				continue;
			}

			if(strcasecmp(tok, name) == 0) {
				fclose(fp);
				return 0;
			}
		}
	}

	clearerr(fp);
	fseek(fp, 0, SEEK_END);

	clean_name = alloca(strlen(name) + 1);
	strcpy(clean_name, name);
	sanitize_appname(clean_name);

	fprintf(fp, "\n[%s]\nTitle = %s\n", clean_name, name);
	fclose(fp);
	return 0;
}

static void init_defaults(void)
{
	memset(&cfg, 0, sizeof cfg);
	strcpy(cfg.pause_key, "Pause");
	strcpy(cfg.recenter_key, "Scroll_Lock");
}

#define CFG_DELIM	" =\t\n\r"
static int read_config(const char *fname, struct config *cfg)
{
	FILE *fp;
	char buf[256];
	int lineno = 0;

	if(!(fp = fopen(fname, "r"))) {
		logmsg("can't open config file: %s: %s\n", fname, strerror(errno));
		return -1;
	}

	while(fgets(buf, sizeof buf, fp)) {
		char *line, *tok, *target = 0;

		lineno++;

		line = strip_spaces(buf);
		if(!line || !*line || *line == '#') {
			continue;
		}

		if(!(tok = strtok(line, CFG_DELIM))) {
			logmsg("%s line %d: malformed line: \"%s\"\n", fname, lineno, line);
			continue;
		}
		if(strcasecmp(tok, "recenter") == 0) {
			target = cfg->recenter_key;
		} else if(strcasecmp(tok, "pause") == 0) {
			target = cfg->pause_key;
		} else {
			logmsg("%s line %d: ignoring unknown: \"%s\"\n", fname, lineno, tok);
			continue;
		}

		if(!(tok = strtok(0, CFG_DELIM))) {
			*target = 0;	/* leaves unbound */
		}
		strcpy(target, tok);
	}

	fclose(fp);
	return 0;
}


static struct appdb_node *create_appdb(struct xml_node *xml)
{
	struct xml_node *iter;
	struct appdb_node *appdb = 0;

	if(strcasecmp(xml->name, "games") != 0) {
		logmsg("Invalid appdb file, root element should be \"Games\", not: %s\n", xml->name);
		return 0;
	}

	iter = xml->chld;
	while(iter) {
		int id;
		struct xml_node *node;
		struct xml_attr *attr;

		node = iter;
		iter = iter->next;

		if(strcasecmp(node->name, "game") != 0) {
			logmsg("appdb: ignoring unexpected element: %s\n", node->name);
			continue;
		}

		if(!(attr = xml_get_attr_case(node, "id"))) {
			logmsg("appdb: game element missing id attribute, ignoring\n");
			continue;
		}
		if(attr->type != ATYPE_INT) {
			logmsg("appdb: game element with invalid id attribute (%s), ignoring\n", attr->str);
			continue;
		}
		id = attr->ival;

		if(!(attr = xml_get_attr_case(node, "name"))) {
			logmsg("appdb: game %d missing name attribute, ignoring\n", id);
			continue;
		}

		appdb_add(&appdb, id, attr->str);
	}

	return appdb;
}

static int appdb_add(struct appdb_node **treeptr, int id, const char *name)
{
	struct appdb_node *node = *treeptr;

	if(!node) {
		if(!(node = malloc(sizeof *node)) || !(node->name = malloc(strlen(name) + 1))) {
			free(node);
			return -1;
		}
		strcpy(node->name, name);
		node->id = id;
		node->left = node->right = 0;
		*treeptr = node;
		return 0;
	}

	if(id == node->id) {
		if(strcmp(name, node->name) != 0) {
			logmsg("appdb: id %d assigned to both \"%s\" and \"%s\" in the app data file!\n", id, node->name, name);
			return -1;
		}
		return 0;	/* duplicate entry */
	}

	return appdb_add(id < node->id ? &node->left : &node->right, id, name);
}

static const char *appdb_find(struct appdb_node *tree, int id)
{
	if(!tree) {
		return 0;
	}

	if(id < tree->id) {
		return appdb_find(tree->left, id);
	}
	if(id > tree->id) {
		return appdb_find(tree->right, id);
	}
	return tree->name;
}

static void appdb_free(struct appdb_node *tree)
{
	if(tree) {
		appdb_free(tree->left);
		appdb_free(tree->right);
		free(tree->name);
		free(tree);
	}
}

static int appdb_count_nodes(struct appdb_node *tree)
{
	if(!tree) return 0;

	return 1 + appdb_count_nodes(tree->left) + appdb_count_nodes(tree->right);
}

static char *strip_spaces(char *buf)
{
	char *end;

	/* skip leading spaces */
	while(*buf && isspace(*buf)) buf++;
	if(!*buf) {
		return 0;
	}

	/* skip trailing spaces */
	end = buf + strlen(buf);
	while(--end > buf && isspace(*end)) *end = 0;
	if(end <= buf) {
		return 0;
	}

	return buf;
}

static void sanitize_appname(char *name)
{
	if(!name) return;

	while(*name) {
		if(isspace(*name) || ispunct(*name)) {
			*name = '_';
		}
		name++;
	}
}
