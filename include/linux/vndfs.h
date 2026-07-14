#ifndef KSU_VNDFS_H
#define KSU_VNDFS_H

#include <linux/version.h>
#include <linux/types.h>
#include <linux/utsname.h>
#include <linux/hashtable.h>
#include <linux/jump_label.h>
#include <linux/path.h>
#include <linux/vndfs_def.h>
#include <linux/statfs.h>

struct filename;
struct seq_file;

#define VNDFS_VERSION "v2.2.0"
#if LINUX_VERSION_CODE < KERNEL_VERSION(5,0,0)
#define VNDFS_VARIANT "NON-GKI"
#else
#define VNDFS_VARIANT "GKI"
#endif

/*********/
/* MACRO */
/*********/
#define getname_safe(name) (name == NULL ? ERR_PTR(-EINVAL) : getname(name))
#define putname_safe(name) (IS_ERR(name) ? NULL : putname(name))

/********/
/* ENUM */
/********/
enum UID_SCHEME {
	UID_NON_APP_PROC = 0,
	UID_ROOT_PROC_EXCEPT_SU_PROC,
	UID_NON_SU_PROC,
	UID_UMOUNTED_APP_PROC,
	UID_UMOUNTED_PROC,
};

/**********/
/* STRUCT */
/**********/
/* sus_path */
#ifdef CONFIG_KSU_SUSFS_SUS_PATH
struct st_vndfs_sus_path {
	char                                    target_pathname[VNDFS_MAX_LEN_PATHNAME];
	int                                     err;
};

struct st_vndfs_sus_path_list {
	struct list_head                        list;
	struct st_vndfs_sus_path                info;
	char                                    target_pathname[VNDFS_MAX_LEN_PATHNAME];
};
#endif

/* sus_mount */
#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
struct st_vndfs_hide_sus_mnts_for_non_su_procs {
	bool                                    enabled;
	int                                     err;
};
#endif // #ifdef CONFIG_KSU_SUSFS_SUS_MOUNT

/* sus_kstat */
#ifdef CONFIG_KSU_SUSFS_SUS_KSTAT
#define KSTAT_SPOOF_INO (1 << 0)
#define KSTAT_SPOOF_DEV (1 << 1)
#define KSTAT_SPOOF_NLINK (1 << 2)
#define KSTAT_SPOOF_SIZE (1 << 3)
#define KSTAT_SPOOF_ATIME_TV_SEC (1 << 4)
#define KSTAT_SPOOF_ATIME_TV_NSEC (1 << 5)
#define KSTAT_SPOOF_MTIME_TV_SEC (1 << 6)
#define KSTAT_SPOOF_MTIME_TV_NSEC (1 << 7)
#define KSTAT_SPOOF_CTIME_TV_SEC (1 << 8)
#define KSTAT_SPOOF_CTIME_TV_NSEC (1 << 9)
#define KSTAT_SPOOF_BLOCKS (1 << 10)
#define KSTAT_SPOOF_BLKSIZE (1 << 11)

struct st_vndfs_sus_kstat {
	int                                     is_statically;
	unsigned long                           target_ino;
	char                                    target_pathname[VNDFS_MAX_LEN_PATHNAME];
	unsigned long                           spoofed_ino;
	unsigned long                           spoofed_dev;
	unsigned int                            spoofed_nlink;
	long long                               spoofed_size;
	long                                    spoofed_atime_tv_sec;
	unsigned long                           spoofed_atime_tv_nsec;
	long                                    spoofed_mtime_tv_sec;
	unsigned long                           spoofed_mtime_tv_nsec;
	long                                    spoofed_ctime_tv_sec;
	unsigned long                           spoofed_ctime_tv_nsec;
	long long                               spoofed_blocks;
	long                                    spoofed_blksize;
	int                                     flags;
	int                                     err;
};

struct st_vndfs_sus_kstat_hlist {
	unsigned long                           target_ino;
	unsigned long                           target_dev;
	bool                                    is_fuse;
	struct st_vndfs_sus_kstat               info;
	struct hlist_node                       node;
};
#endif

/* spoof_uname */
#ifdef CONFIG_KSU_SUSFS_SPOOF_UNAME
struct st_vndfs_uname {
	char                                    release[__NEW_UTS_LEN+1];
	char                                    version[__NEW_UTS_LEN+1];
	int                                     err;
};
#endif

/* enable_log */
#ifdef CONFIG_KSU_SUSFS_ENABLE_LOG
struct st_vndfs_log {
	bool                                    enabled;
	int                                     err;
};
#endif

/* spoof_cmdline_or_bootconfig */
#ifdef CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG
struct st_vndfs_spoof_cmdline_or_bootconfig {
	char                                    fake_cmdline_or_bootconfig[VNDFS_FAKE_CMDLINE_OR_BOOTCONFIG_SIZE];
	int                                     err;
};
#endif

/* open_redirect */
#ifdef CONFIG_KSU_SUSFS_OPEN_REDIRECT
struct st_vndfs_open_redirect {
	char                                    target_pathname[VNDFS_MAX_LEN_PATHNAME];
	char                                    redirected_pathname[VNDFS_MAX_LEN_PATHNAME];
	int                                     uid_scheme;
	int                                     err;
};

struct st_vndfs_open_redirect_hlist {
	unsigned long                           target_ino;
	unsigned long                           target_dev;
	unsigned long                           redirected_ino;
	unsigned long                           redirected_dev;
	int                                     spoofed_mnt_id;
	struct kstatfs                          spoofed_kstatfs;
	struct st_vndfs_open_redirect           info;
	bool                                    reversed_lookup_only;
	struct hlist_node                       node;
};
#endif

/* sus_map */
#ifdef CONFIG_KSU_SUSFS_SUS_MAP
struct st_vndfs_sus_map {
	char                                    target_pathname[VNDFS_MAX_LEN_PATHNAME];
	int                                     err;
};
#endif

/* sus_su compatibility */
struct st_sus_su {
	int                                     mode;
	int                                     err;
};

/* avc log spoofing */
struct st_vndfs_avc_log_spoofing {
	bool                                    enabled;
	int                                     err;
};

/* get enabled features */
struct st_vndfs_enabled_features {
	char                                    enabled_features[VNDFS_ENABLED_FEATURES_SIZE];
	int                                     err;
};

/* show variant */
struct st_vndfs_variant {
	char                                    vndfs_variant[16];
	int                                     err;
};

/* show version */
struct st_vndfs_version {
	char                                    vndfs_version[16];
	int                                     err;
};

/***********************/
/* FORWARD DECLARATION */
/***********************/
/* sus_path */
#ifdef CONFIG_KSU_SUSFS_SUS_PATH
void vndfs_add_sus_path(void __user **user_info);
void vndfs_add_sus_path_loop(void __user **user_info);
void vndfs_run_sus_path_loop(void);
int vndfs_sus_ino_for_filldir64(unsigned long ino);
int vndfs_get_data_path(struct path *path);
#endif

/* sus_mount */
#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
void vndfs_set_hide_sus_mnts_for_non_su_procs(void __user **user_info);
#endif // #ifdef CONFIG_KSU_SUSFS_SUS_MOUNT

/* sus_kstat */
#ifdef CONFIG_KSU_SUSFS_SUS_KSTAT
void vndfs_add_sus_kstat(void __user **user_info);
void vndfs_update_sus_kstat(void __user **user_info);
void vndfs_sus_ino_for_generic_fillattr(unsigned long ino, struct kstat *stat);
void vndfs_sus_ino_for_show_map_vma(unsigned long ino, dev_t *out_dev, unsigned long *out_ino);
#endif

/* spoof_uname */
#ifdef CONFIG_KSU_SUSFS_SPOOF_UNAME
void vndfs_set_uname(void __user **user_info);
void vndfs_spoof_uname(struct new_utsname* tmp);
#endif

/* enable_log */
#ifdef CONFIG_KSU_SUSFS_ENABLE_LOG
void vndfs_enable_log(void __user **user_info);
#endif

/* spoof_cmdline_or_bootconfig */
#ifdef CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG
void vndfs_set_cmdline_or_bootconfig(void __user **user_info);
int vndfs_spoof_cmdline_or_bootconfig(struct seq_file *m);
#endif

/* open_redirect */
#ifdef CONFIG_KSU_SUSFS_OPEN_REDIRECT
void vndfs_add_open_redirect(void __user **user_info);
struct filename *vndfs_open_redirect_spoof_do_sys_openat(struct inode *inode);
struct filename *vndfs_get_redirected_path(unsigned long ino);
int vndfs_open_redirect_spoof_vfs_readlink(struct inode *inode, char __user *buffer, int buflen);
int vndfs_open_redirect_spoof_do_proc_readlink(struct inode *inode, char *tmp_buf, int buflen);
int vndfs_open_redirect_spoof_vfs_statfs(struct inode *inode, struct kstatfs *buf);
int vndfs_open_redirect_spoof_seq_show(struct inode *inode, int *out_mnt_id, unsigned long *out_ino);
int vndfs_open_redirect_spoof_show_map_vma(struct inode *inode, unsigned long *out_ino, dev_t *out_dev, char *spoofed_name);
#endif

/* sus_map */
#ifdef CONFIG_KSU_SUSFS_SUS_MAP
void vndfs_add_sus_map(void __user **user_info);
#endif

void vndfs_set_avc_log_spoofing(void __user **user_info);
DECLARE_STATIC_KEY_FALSE(vndfs_is_avc_log_spoofing_enabled);

void vndfs_get_enabled_features(void __user **user_info);
void vndfs_show_variant(void __user **user_info);
void vndfs_show_version(void __user **user_info);

void vndfs_start_sdcard_monitor_fn(void);
bool vndfs_is_allow_su(void);
void vndfs_try_umount(uid_t uid);
int vndfs_get_sus_su_working_mode(void);
void vndfs_sus_su(void __user **user_info);

/* vndfs_init */
void vndfs_init(void);
bool vndfs_handle_ioctl(unsigned int cmd, unsigned long arg);

#endif
