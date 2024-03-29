#ifndef _START_EXEC_H
#define _START_EXEC_H
#include "license_pbs.h" /* See here for the software license */
#include <pwd.h> /* passwd, gid_t */

#include "pbs_job.h" /* job, task, hnodent, pjobexec_t, task */
#include "mom_func.h" /* var_table, radix_buf */
#include "list_link.h" /* tlist_head */
#include "libpbs.h" /* job_file */

#ifdef ENABLE_CSA
/* valid commands for csaswitch checking */
enum csa_chk_cmd
  {
  IS_INSTALLED = 0,
  IS_UP = 1
  };
#endif /* ENABLE_CSA */


/* static void no_hang(int sig); */

struct passwd *check_pwd(job *pjob);

void exec_bail(job *pjob, int code);

int open_demux(u_long addr, int port);

/* static int open_pty(job *pjob); */

int is_joined(job *pjob);

/* static int open_std_out_err(job *pjob, int timeout); */

int mkdirtree(char *dirpath, mode_t mode);

int TTmpDirName(job *pjob, char *tmpdir);

int TMakeTmpDir(job *pjob, char *tmpdir);

int InitUserEnv(job *pjob, task *ptask, char **envp, struct passwd *pwdp, char *shell);

int mom_jobstarter_execute_job(job *pjob, char *shell, char *arg[], struct var_table *vtable);

int open_tcp_stream_to_sisters(job *pjob, int com, tm_event_t parent_event, int mom_radix, hnodent *hosts, struct radix_buf **sister_list, tlist_head *phead, int flag);

void free_sisterlist(struct radix_buf **list, int radix);

struct radix_buf **allocate_sister_list(int radix);

int TMomFinalizeJob1(job *pjob, pjobexec_t *TJE, int *SC);

int TMomFinalizeJob2(pjobexec_t *TJE, int *SC);

int determine_umask(int uid);

#ifdef PENABLE_LINUX26_CPUSETS
int use_cpusets(job *pjob);
#endif /* PENABLE_LINUX26_CPUSETS */
int write_gpus_to_file(job *pjob);

int write_nodes_to_file(job *pjob);

int TMomFinalizeChild(pjobexec_t *TJE);

int TMomFinalizeJob3(pjobexec_t *TJE, int ReadSize, int ReadErrno, int *SC);

int start_process(task *ptask, char **argv, char **envp);

void nodes_free(job *pj);

int add_host_to_sister_list(char *hostname, unsigned short port, struct radix_buf *list);

void job_nodes(job *pjob);

void sister_job_nodes(job *pjob, char *radix_hosts, char *radix_ports );

int  start_exec(job *pjob);

pid_t fork_me(int conn);

/* static void starter_return(int upfds, int downfds, int code, struct startjob_rtn *sjrtn); */

int remove_leading_hostname(char **jobpath);

char *std_file_name(job *pjob, enum job_file which, int *keeping);

int open_std_file(job *pjob, enum job_file which, int mode, gid_t exgid);

/* static int find_env_slot(struct var_table *ptbl, char *pstr); */

void bld_env_variables(struct var_table *vtable, char *name, char *value);

#ifndef __TOLDGROUP
int init_groups(char *pwname, int pwgrp, int groupsize, int *groups);
#else /* !__TOLDGROUP */
int init_groups(char *pwname, int pwgrp, int groupsize, int *groups);
#endif /* !__TOLDGROUP */

/* static void catchinter(int sig); */

/* static int search_env_and_open(const char *envname, u_long ipaddr); */

int TMomCheckJobChild(pjobexec_t *TJE, int Timeout, int *Count, int *RC);

#ifdef USEJOBCREATE
uint64_t get_jobid(char* pbs_jobid);
#endif /* USEJOBCREATE */

#ifdef ENABLE_CSA
int check_csa_status(enum csa_chk_cmd chk_action);

int create_WLM_Rec(char *pbs_jobid, uint64_t job_id, int type, int subtype, uint64_t prid, uint64_t ash, int64_t compCode, int64_t reqid);

void add_wkm_start(uint64_t job_id, char* pbs_jobid);

void add_wkm_end(uint64_t job_id, int64_t comp_code, char *pbs_jobid);
#endif /* ENABLE_CSA */

int expand_path(job *pjob, char *path_in, int pathlen, char *path);

int exec_job_on_ms(job *pjob);

int allocate_demux_sockets(job *pjob, int flag);

#endif /* _START_EXEC_H */
