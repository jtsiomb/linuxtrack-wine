#ifndef CFG_H_
#define CFG_H_

struct appdb_node;

struct config {
	char pause_key[32];
	char recenter_key[32];
	struct appdb_node *appdb;
};

int init_config(void);
void cleanup_config(void);

struct config *get_config(void);

const char *app_name(int app_id);

int add_ltr_profile(const char *name);

#endif	/* CFG_H_ */
