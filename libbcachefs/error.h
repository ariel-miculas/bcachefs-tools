#ifndef _BCACHE_ERROR_H
#define _BCACHE_ERROR_H

#include <linux/printk.h>

struct bch_dev;
struct bch_fs;
struct work_struct;

/*
 * XXX: separate out errors that indicate on disk data is inconsistent, and flag
 * superblock as such
 */

/* Error messages: */

/*
 * Very fatal logic/inconsistency errors: these indicate that we've majorly
 * screwed up at runtime, i.e. it's not likely that it was just caused by the
 * data on disk being inconsistent. These BUG():
 *
 * XXX: audit and convert to inconsistent() checks
 */

#define bch2_fs_bug(c, ...)						\
do {									\
	bch_err(c, __VA_ARGS__);					\
	BUG();								\
} while (0)

#define bch2_fs_bug_on(cond, c, ...)					\
do {									\
	if (cond)							\
		bch2_fs_bug(c, __VA_ARGS__);				\
} while (0)

/*
 * Inconsistency errors: The on disk data is inconsistent. If these occur during
 * initial recovery, they don't indicate a bug in the running code - we walk all
 * the metadata before modifying anything. If they occur at runtime, they
 * indicate either a bug in the running code or (less likely) data is being
 * silently corrupted under us.
 *
 * XXX: audit all inconsistent errors and make sure they're all recoverable, in
 * BCH_ON_ERROR_CONTINUE mode
 */

void bch2_inconsistent_error(struct bch_fs *);

#define bch2_fs_inconsistent(c, ...)					\
do {									\
	bch_err(c, __VA_ARGS__);					\
	bch2_inconsistent_error(c);					\
} while (0)

#define bch2_fs_inconsistent_on(cond, c, ...)				\
({									\
	int _ret = !!(cond);						\
									\
	if (_ret)							\
		bch2_fs_inconsistent(c, __VA_ARGS__);			\
	_ret;								\
})

/*
 * Later we might want to mark only the particular device inconsistent, not the
 * entire filesystem:
 */

#define bch2_dev_inconsistent(ca, ...)					\
do {									\
	bch_err(ca, __VA_ARGS__);					\
	bch2_inconsistent_error((ca)->fs);				\
} while (0)

#define bch2_dev_inconsistent_on(cond, ca, ...)				\
({									\
	int _ret = !!(cond);						\
									\
	if (_ret)							\
		bch2_dev_inconsistent(ca, __VA_ARGS__);			\
	_ret;								\
})

/*
 * Fsck errors: inconsistency errors we detect at mount time, and should ideally
 * be able to repair:
 */

enum {
	BCH_FSCK_OK			= 0,
	BCH_FSCK_ERRORS_NOT_FIXED	= 1,
	BCH_FSCK_REPAIR_UNIMPLEMENTED	= 2,
	BCH_FSCK_REPAIR_IMPOSSIBLE	= 3,
	BCH_FSCK_UNKNOWN_VERSION	= 4,
};

/* These macros return true if error should be fixed: */

/* XXX: mark in superblock that filesystem contains errors, if we ignore: */

enum fsck_err_opts {
	FSCK_ERR_NO,
	FSCK_ERR_YES,
	FSCK_ERR_ASK,
};

#ifdef __KERNEL__
#define __fsck_err_should_fix(c, msg, ...)				\
({									\
	bool _fix = (c)->opts.fix_errors;				\
	bch_err(c, msg ", %sfixing", ##__VA_ARGS__, _fix ? "" : "not ");\
	_fix;								\
})
#else
#include "tools-util.h"

#define __fsck_err_should_fix(c, msg, ...)				\
({									\
	bool _fix = false;						\
	switch ((c)->opts.fix_errors) {					\
	case FSCK_ERR_ASK:						\
		printf(msg ": fix?", ##__VA_ARGS__);			\
		_fix = ask_yn();					\
		break;							\
	case FSCK_ERR_YES:						\
		bch_err(c, msg ", fixing", ##__VA_ARGS__);		\
		_fix = true;						\
		break;							\
	case FSCK_ERR_NO:						\
		bch_err(c, msg, ##__VA_ARGS__);				\
		_fix = false;						\
		break;							\
	}								\
	_fix;								\
})
#endif

#define __fsck_err(c, _can_fix, _can_ignore, _nofix_msg, msg, ...)	\
({									\
	bool _fix;							\
									\
	if (_can_fix) {							\
		_fix = __fsck_err_should_fix(c, msg, ##__VA_ARGS__);	\
	} else {							\
		bch_err(c, msg " ("_nofix_msg")", ##__VA_ARGS__);	\
		_fix = false;						\
	}								\
									\
	if (_fix)							\
		set_bit(BCH_FS_FSCK_FIXED_ERRORS, &(c)->flags);		\
									\
	if (!_fix && !_can_ignore) {					\
		bch_err(c, "Unable to continue, halting");		\
		ret = BCH_FSCK_ERRORS_NOT_FIXED;			\
		goto fsck_err;						\
	}								\
									\
	_fix;								\
})

#define __fsck_err_on(cond, c, _can_fix, _can_ignore, _nofix_msg, ...)	\
	((cond) ? __fsck_err(c, _can_fix, _can_ignore,			\
			     _nofix_msg, ##__VA_ARGS__) : false)

#define unfixable_fsck_err_on(cond, c, ...)				\
	__fsck_err_on(cond, c, false, true, "repair unimplemented", ##__VA_ARGS__)

#define need_fsck_err_on(cond, c, ...)					\
	__fsck_err_on(cond, c, false, true, "run fsck to correct", ##__VA_ARGS__)

#define mustfix_fsck_err(c, ...)					\
	__fsck_err(c, true, false, "not fixing", ##__VA_ARGS__)

#define mustfix_fsck_err_on(cond, c, ...)				\
	__fsck_err_on(cond, c, true, false, "not fixing", ##__VA_ARGS__)

#define fsck_err_on(cond, c, ...)					\
	__fsck_err_on(cond, c, true, true, "not fixing", ##__VA_ARGS__)

/*
 * Fatal errors: these don't indicate a bug, but we can't continue running in RW
 * mode - pretty much just due to metadata IO errors:
 */

void bch2_fatal_error(struct bch_fs *);

#define bch2_fs_fatal_error(c, ...)					\
do {									\
	bch_err(c, __VA_ARGS__);					\
	bch2_fatal_error(c);						\
} while (0)

#define bch2_fs_fatal_err_on(cond, c, ...)				\
({									\
	int _ret = !!(cond);						\
									\
	if (_ret)							\
		bch2_fs_fatal_error(c, __VA_ARGS__);			\
	_ret;								\
})

#define bch2_dev_fatal_error(ca, ...)					\
do {									\
	bch_err(ca, __VA_ARGS__);					\
	bch2_fatal_error(c);						\
} while (0)

#define bch2_dev_fatal_io_error(ca, fmt, ...)				\
do {									\
	printk_ratelimited(KERN_ERR bch2_fmt((ca)->fs,			\
		"fatal IO error on %s for " fmt),			\
		(ca)->name, ##__VA_ARGS__);				\
	bch2_fatal_error((ca)->fs);					\
} while (0)

#define bch2_dev_fatal_io_err_on(cond, ca, ...)				\
({									\
	int _ret = !!(cond);						\
									\
	if (_ret)							\
		bch2_dev_fatal_io_error(ca, __VA_ARGS__);		\
	_ret;								\
})

/*
 * Nonfatal IO errors: either recoverable metadata IO (because we have
 * replicas), or data IO - we need to log it and print out a message, but we
 * don't (necessarily) want to shut down the fs:
 */

void bch2_nonfatal_io_error_work(struct work_struct *);

/* Does the error handling without logging a message */
void bch2_nonfatal_io_error(struct bch_dev *);

#if 0
#define bch2_fs_nonfatal_io_error(c, ...)				\
do {									\
	bch_err(c, __VA_ARGS__);					\
	bch2_nonfatal_io_error(c);					\
} while (0)
#endif

/* Logs message and handles the error: */
#define bch2_dev_nonfatal_io_error(ca, fmt, ...)				\
do {									\
	printk_ratelimited(KERN_ERR bch2_fmt((ca)->fs,			\
		"IO error on %s for " fmt),				\
		(ca)->name, ##__VA_ARGS__);				\
	bch2_nonfatal_io_error(ca);					\
} while (0)

#define bch2_dev_nonfatal_io_err_on(cond, ca, ...)			\
({									\
	bool _ret = (cond);						\
									\
	if (_ret)							\
		bch2_dev_nonfatal_io_error(ca, __VA_ARGS__);		\
	_ret;								\
})

/* kill? */

#define __bcache_io_error(c, fmt, ...)					\
	printk_ratelimited(KERN_ERR bch2_fmt(c,				\
			"IO error: " fmt), ##__VA_ARGS__)

#define bcache_io_error(c, bio, fmt, ...)				\
do {									\
	__bcache_io_error(c, fmt, ##__VA_ARGS__);			\
	(bio)->bi_error = -EIO;						\
} while (0)

#endif /* _BCACHE_ERROR_H */
