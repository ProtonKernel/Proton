/* SPDX-License-Identifier: GPL-2.0 */
/*
 * add macros and functions to support 5.10 Kunit
 *
 */

#define kunit_log(lvl, test_or_suite, fmt, ...)				\
	do {								\
		printk(lvl fmt, ##__VA_ARGS__);				\
	} while (0)

// struct test
struct kunit {
	void *priv;
	/* private: internal use only. */
	spinlock_t lock; /* Guards all mutable test state. */
	struct list_head resources;
	struct list_head post_conditions;
	const char *name;
	char *log;
	bool death_test;
	bool success;
	void (*vprintk)(const struct test *test,
			const char *level,
			struct va_format *vaf);
	void (*fail)(struct test *test, struct test_stream *stream);
	void (*abort)(struct test *test);
	struct test_try_catch try_catch;
};

typedef void (*kunit_try_catch_func_t)(void *);

// struct test_try_catch
struct kunit_try_catch {
	/* private: internal use only. */
	void (*run)(struct kunit_try_catch *try_catch);
	void __noreturn (*throw)(struct kunit_try_catch *try_catch);
	struct kunit *test;
	struct completion *try_completion;
	int try_result;
	kunit_try_catch_func_t try;
	kunit_try_catch_func_t catch;
	void *context;
};

// struct test_case
struct kunit_case {
	void (*run_case)(struct kunit *test);
	const char name[256];
	/* private: internal use only. */
	bool success;
};

// struct test_module
struct kunit_suite {
	const char name[256];
	// FIX ME
	char *log;
	int (*init)(struct kunit *test);
	void (*exit)(struct kunit *test);
	struct kunit_case *test_cases;
};

struct kunit_resource;
typedef void (*kunit_resource_free_t)(struct kunit_resource *);
typedef int (*kunit_resource_init_t)(struct kunit_resource *, void *);

struct kunit_resource {
	void *data;
	const char *name;
	kunit_resource_free_t free;

	/* private: internal use only. */
	struct kref refcount;
	struct list_head node;
};

static inline int kunit_add_resource(struct kunit *test,
		       kunit_resource_init_t init,
		       kunit_resource_free_t free,
		       struct kunit_resource *res,
		       void *data)
{
	int ret = 0;

	res->free = free;
	kref_init(&res->refcount);

	if (init) {
		ret = init(res, data);
		if (ret)
			return ret;
	} else {
		res->data = data;
	}

	spin_lock(&test->lock);
	list_add_tail(&res->node, &test->resources);
	/* refcount for list is established by kref_init() */
	spin_unlock(&test->lock);

	return ret;
}
static inline void kunit_put_resource(struct kunit_resource *res);
static inline struct kunit_resource *
kunit_find_named_resource(struct kunit *test,
			  const char *name);

static inline int kunit_add_named_resource(struct kunit *test,
			     kunit_resource_init_t init,
			     kunit_resource_free_t free,
			     struct kunit_resource *res,
			     const char *name,
			     void *data)
{
	struct kunit_resource *existing;

	if (!name)
		return -EINVAL;

	existing = kunit_find_named_resource(test, name);

	if (existing) {
		kunit_put_resource(existing);
		return -EEXIST;
	}

	res->name = name;

	return kunit_add_resource(test, init, free, res, data);
}

static inline void kunit_get_resource(struct kunit_resource *res)
{
	kref_get(&res->refcount);
}

static inline void kunit_release_resource(struct kref *kref)
{
	struct kunit_resource *res = container_of(kref, struct kunit_resource,
						  refcount);

	/* If free function is defined, resource was dynamically allocated. */
	if (res->free) {
		res->free(res);
		kfree(res);
	}
}

static inline void kunit_put_resource(struct kunit_resource *res)
{
	kref_put(&res->refcount, kunit_release_resource);
}

typedef bool (*kunit_resource_match_t)(struct kunit *test,
				       struct kunit_resource *res,
				       void *match_data);

static inline bool kunit_resource_name_match(struct kunit *test,
					     struct kunit_resource *res,
					     void *match_name)
{
	return res->name && strcmp(res->name, match_name) == 0;
}

static inline struct kunit_resource *
kunit_find_resource(struct kunit *test,
		    kunit_resource_match_t match,
		    void *match_data)
{
	struct kunit_resource *res, *found = NULL;

	spin_lock(&test->lock);
	list_for_each_entry_reverse(res, &test->resources, node) {
		if (match(test, res, (void *)match_data)) {
			found = res;
			kunit_get_resource(found);
			break;
		}
	}
	spin_unlock(&test->lock);

	return found;
}

static inline struct kunit_resource *
kunit_find_named_resource(struct kunit *test,
			  const char *name)
{
	return kunit_find_resource(test, kunit_resource_name_match,
				   (void *)name);
}

static inline void kunit_remove_resource(struct kunit *test, struct kunit_resource *res)
{
	spin_lock(&test->lock);
	list_del(&res->node);
	spin_unlock(&test->lock);
	kunit_put_resource(res);
}

static inline int kunit_destroy_resource(struct kunit *test, kunit_resource_match_t match,
			   void *match_data)
{
	struct kunit_resource *res = kunit_find_resource(test, match,
							 match_data);

	if (!res)
		return -ENOENT;

	kunit_remove_resource(test, res);

	/* We have a reference also via _find(); drop it. */
	kunit_put_resource(res);

	return 0;
}

static inline int kunit_destroy_named_resource(struct kunit *test,
					       const char *name)
{
	return kunit_destroy_resource(test, kunit_resource_name_match,
				      (void *)name);
}

static inline bool kunit_resource_instance_match(struct kunit *test,
						 struct kunit_resource *res,
						 void *match_data)
{
	return res->data == match_data;
}

static inline struct kunit_resource *kunit_alloc_and_get_resource(struct kunit *test,
						    kunit_resource_init_t init,
						    kunit_resource_free_t free,
						    gfp_t internal_gfp,
						    void *data)
{
	struct kunit_resource *res;
	int ret;

	res = kzalloc(sizeof(*res), internal_gfp);
	if (!res)
		return NULL;

	ret = kunit_add_resource(test, init, free, res, data);
	if (!ret) {
		/*
		 * bump refcount for get; kunit_resource_put() should be called
		 * when done.
		 */
		kunit_get_resource(res);
		return res;
	}
	return NULL;
}

static inline void kunit_cleanup(struct kunit *test)
{
	struct kunit_resource *res;

	/*
	 * test->resources is a stack - each allocation must be freed in the
	 * reverse order from which it was added since one resource may depend
	 * on another for its entire lifetime.
	 * Also, we cannot use the normal list_for_each constructs, even the
	 * safe ones because *arbitrary* nodes may be deleted when
	 * kunit_resource_free is called; the list_for_each_safe variants only
	 * protect against the current node being deleted, not the next.
	 */
	while (true) {
		spin_lock(&test->lock);
		if (list_empty(&test->resources)) {
			spin_unlock(&test->lock);
			break;
		}
		res = list_last_entry(&test->resources,
				      struct kunit_resource,
				      node);
		/*
		 * Need to unlock here as a resource may remove another
		 * resource, and this can't happen if the test->lock
		 * is held.
		 */
		spin_unlock(&test->lock);
		kunit_remove_resource(test, res);
	}
#if (IS_ENABLED(CONFIG_KASAN) && IS_ENABLED(CONFIG_KUNIT))
	current->kunit_test = NULL;
#endif /* IS_ENABLED(CONFIG_KASAN) && IS_ENABLED(CONFIG_KUNIT)*/
}

static inline void kunit_init_test(struct kunit *test, const char *name, char *log)
{
	spin_lock_init(&test->lock);
	INIT_LIST_HEAD(&test->resources);
	INIT_LIST_HEAD(&test->post_conditions);
	test->name = name;

	test->log = log;
	if (test->log)
		test->log[0] = '\0';

	test->success = true;
}

static inline void *kunit_alloc_resource(struct kunit *test,
					 kunit_resource_init_t init,
					 kunit_resource_free_t free,
					 gfp_t internal_gfp,
					 void *context)
{
	struct kunit_resource *res;

	res = kzalloc(sizeof(*res), internal_gfp);
	if (!res)
		return NULL;

	if (!kunit_add_resource(test, init, free, res, context))
		return res->data;

	return NULL;
}

#define kunit_info(kunit_test, ...)  test_info((struct test *)kunit_test, ##__VA_ARGS__)
#define kunit_warn test_warn
#define kunit_err test_err
#define kunit_printk test_printk

#define kunit_kzalloc(kunit_test, ...) test_kzalloc((struct test *)kunit_test, ##__VA_ARGS__)
#define kunit_kmalloc(kunit_test, ...) test_kmalloc((struct test *)kunit_test, ##__VA_ARGS__)

// strng-stream.c
struct string_stream_alloc_context {
	struct kunit *test;
	gfp_t gfp;
};

static inline int string_stream_init(struct kunit_resource *res, void *context)
{
	struct string_stream *stream;
	struct string_stream_alloc_context *ctx = context;
	struct kunit *stream_test = ctx->test;

	stream = kunit_kzalloc(stream_test, sizeof(*stream), ctx->gfp);
	if (!stream)
		return -ENOMEM;

	res->data = stream;
	stream->gfp = ctx->gfp;
	stream->test = (struct test *)ctx->test;
	INIT_LIST_HEAD(&stream->fragments);
	spin_lock_init(&stream->lock);

	return 0;
}

static void string_stream_free(struct kunit_resource *res)
{
	struct string_stream *stream = res->data;

	string_stream_clear(stream);
}

static inline struct string_stream *alloc_string_stream(struct kunit *test, gfp_t gfp)
{
	struct string_stream_alloc_context context = {
		.test = test,
		.gfp = gfp
	};

	return kunit_alloc_resource(test,
				    string_stream_init,
				    string_stream_free,
				    gfp,
				    &context);
}

// kunit_test_suite
#define kunit_test_suite module_test

/*
 * NOTE:
 * This macro is come from alpha/master
 * 53393b9 2021-01-28 06:13 Daniel Latypov ● kunit: alpha: transition: make kunit_test_suites() take pointers
 *
 * in upstream, this can handle multiple suites. We can't here.
 */
#define ___concat(a, b) a##b
#define __concat(a, b) ___concat(a, b)
#define kunit_test_suites(suite_ptr) \
		static struct kunit_suite *__concat(__test_module_, __COUNTER__) __used \
		__aligned(8) __attribute__((__section__(".test_modules"))) = \
			suite_ptr

#define kunit_try_catch_init(try_catch, kunit_test, try, catch) test_try_catch_init((struct test_try_catch *)try_catch, (struct test *)kunit_test, (struct test_try_catch_func_t)try, (struct test_try_catch_func_t)catch)
#define kunit_try_catch_run(try_catch, kunit_test) test_try_catch_run((struct test_try_catch *)try_catch, (struct test *)kunit_test)
#define kunit_try_catch_throw(try_catch) test_try_catch_throw((struct test_try_catch *)try_catch)

#define KUNIT_CASE(test_name) { .run_case = test_name, .name = #test_name }

#define KUNIT_SUCCEED(kunit_test, ...) SUCCEED((struct test *)kunit_test, ##__VA_ARGS__)
#define KUNIT_FAIL(kunit_test, ...) FAIL((struct test *)kunit_test, ##__VA_ARGS__)

#define KUNIT_EXPECT(kunit_test, ...) EXPECT((struct test *)kunit_test, ##__VA_ARGS__)
#define KUNIT_EXPECT_TRUE(kunit_test, ...) EXPECT_TRUE((struct test *)kunit_test, ##__VA_ARGS__)
#define KUNIT_EXPECT_FALSE(kunit_test, ...) EXPECT_FALSE((struct test *)kunit_test, ##__VA_ARGS__)

#define KUNIT_EXPECT_NOT_NULL(kunit_test, ...) EXPECT_NOT_NULL((struct test *)test, ##__VA_ARGS__)
#define KUNIT_EXPECT_NULL(kunit_test, ...) EXPECT_NULL((struct test *)test, ##__VA_ARGS__)
#define KUNIT_EXPECT_SUCCESS(kunit_test, ...) EXPECT_SUCCESS((struct test *)test, ##__VA_ARGS__)
#define KUNIT_EXPECT_ERROR(kunit_test, ...) EXPECT_ERROR((struct test *)test, ##__VA_ARGS__)
#define KUNIT_EXPECT_BINARY(kunit_test, ...) EXPECT_BINARY((struct test *)kunit_test, ##__VA_ARGS__)

#define KUNIT_EXPECT_EQ(kunit_test, left, right) EXPECT_EQ((struct test *)kunit_test, (left), (right))
#define KUNIT_EXPECT_NE(kunit_test, left, right) EXPECT_NE((struct test *)kunit_test, (left), (right))
#define KUNIT_EXPECT_LT(kunit_test, left, right) EXPECT_LT((struct test *)kunit_test, (left), (right))
#define KUNIT_EXPECT_LE(kunit_test, left, right) EXPECT_LE((struct test *)kunit_test, (left), (right))
#define KUNIT_EXPECT_GT(kunit_test, left, right) EXPECT_GT((struct test *)kunit_test, (left), (right))
#define KUNIT_EXPECT_GE(kunit_test, left, right) EXPECT_GE((struct test *)kunit_test, (left), (right))
#define KUNIT_EXPECT_STREQ(kunit_test, left, right) EXPECT_STREQ((struct test *)kunit_test, (left), (right))

#define KUNIT_EXPECT_PTR_EQ(kunit_test, left, right) EXPECT_EQ((struct test *)kunit_test, (left), (right))
#define KUNIT_EXPECT_PTR_NE(kunit_test, left, right) EXPECT_NE((struct test *)kunit_test, (left), (right))

#define KUNIT_EXPECT_NOT_ERR_OR_NULL(kunit_test, ...) EXPECT_NOT_ERR_OR_NULL((struct test *)kunit_test, ##__VA_ARGS__)

#define KUNIT_ASSERT_TRUE(kunit_test, ...) ASSERT_TRUE((struct test *)kunit_test, ##__VA_ARGS__)
#define KUNIT_ASSERT_FALSE(kunit_test, ...) ASSERT_FALSE((struct test *)kunit_test, ##__VA_ARGS__)

#define KUNIT_ASSERT_NOT_NULL(test, ...) ASSERT_NOT_NULL((struct test *)test, ##__VA_ARGS__)
#define KUNIT_ASSERT_NULL(test, ...) ASSERT_NULL((struct test *)test, ##__VA_ARGS__)
#define KUNIT_ASSERT_SUCCESS(test, ...) ASSERT_SUCCESS((struct test *)test, ##__VA_ARGS__)
#define KUNIT_ASSERT_ERROR(test, ...) ASSERT_ERROR((struct test *)test, ##__VA_ARGS__)
#define KUNIT_ASSERT_BINARY(test, ...) ASSERT_BINARY((struct test *)test, ##__VA_ARGS__)
#define KUNIT_ASSERT_FAILURE(kunit_test, ...) ASSERT_FAILURE((struct test *)kunit_test, ##__VA_ARGS__)

#define KUNIT_ASSERT_EQ(kunit_test, left, right) ASSERT_EQ((struct test *)kunit_test, (left), (right))
#define KUNIT_ASSERT_NE(kunit_test, left, right) ASSERT_NE((struct test *)kunit_test, (left), (right))
#define KUNIT_ASSERT_LT(kunit_test, left, right) ASSERT_LT((struct test *)kunit_test, (left), (right))
#define KUNIT_ASSERT_LE(kunit_test, left, right) ASSERT_LE((struct test *)kunit_test, (left), (right))
#define KUNIT_ASSERT_GT(kunit_test, left, right) ASSERT_GT((struct test *)kunit_test, (left), (right))
#define KUNIT_ASSERT_GE(kunit_test, left, right) ASSERT_GE((struct test *)kunit_test, (left), (right))
#define KUNIT_ASSERT_STREQ(kunit_test, left, right) ASSERT_STREQ((struct test *)kunit_test, (left), (right))

#define KUNIT_ASSERT_PTR_EQ(kunit_test, left, right) ASSERT_PTR_EQ((struct test *)kunit_test, (left), (right))
#define KUNIT_ASSERT_PTR_NE(kunit_test, left, right) ASSERT_PTR_NE((struct test *)kunit_test, (left), (right))

#define KUNIT_ASSERT_NOT_ERR_OR_NULL(kunit_test, ...) ASSERT_NOT_ERR_OR_NULL((struct test *)kunit_test, ##__VA_ARGS__)
