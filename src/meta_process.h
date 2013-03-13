#ifndef META_PROCESS_H
#define META_PROCESS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct process_tag* process;

process process_new(const char* appname);
void process_free(process p);

int process_shutting_down(process p);

int process_set_rootdir(process p, const char* path);
int process_set_username(process p, const char* username);

int process_add_object_to_start(
	process p,
	void *object,
	int do_func(void *),
	int undo_func(void *),
	int run_func(void *),
	int shutdown_func(void *));

int process_start(process p, int fork_and_close);
int process_wait_for_shutdown(process p);
int process_get_exitcode(process p, void* object);


#ifdef __cplusplus
}
#endif

#endif /* guard */

