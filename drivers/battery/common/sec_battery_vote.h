
#ifndef __SEC_VOTER_H
#define __SEC_VOTER_H __FILE__

enum {
	SEC_VOTE_MIN,
	SEC_VOTE_MAX,
	SEC_VOTE_EN,
};

enum {
	VOTE_PRI_0 = 0,
	VOTE_PRI_1,
	VOTE_PRI_2,
	VOTE_PRI_3,
	VOTE_PRI_4,
	VOTE_PRI_5,
	VOTE_PRI_6,
	VOTE_PRI_7,
	VOTE_PRI_8,
	VOTE_PRI_9,
	VOTE_PRI_10,
};
struct sec_vote;

extern int get_sec_vote(struct sec_vote *vote, const char **name, int *value);
extern struct sec_vote *sec_vote_init(const char *name, int type, int num, int init_val,
		const char **voter_name, int(*cb)(void *data, int value), void *data);
extern void sec_vote_destroy(struct sec_vote *vote);
extern void _sec_vote(struct sec_vote *vote, int event, int en, int value, const char *fname, int line);
extern void sec_vote_refresh(struct sec_vote *vote);
extern const char *get_sec_keyvoter_name(struct sec_vote *vote);
extern int get_sec_vote_result(struct sec_vote *vote);
extern int get_sec_voter_status(struct sec_vote *vote, int id, int *v);
extern int show_sec_vote_status(char *buf, unsigned int p_size);
extern void change_sec_voter_pri(struct sec_vote *vote, int event, int pri);

#define sec_vote(vote, event, en, value)	_sec_vote(vote, event, en, value, __func__, __LINE__)

#endif
