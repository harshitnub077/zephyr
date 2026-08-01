/*
 * Copyright (c) 2024, Meta
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <pthread.h>

#include <zephyr/sys/util.h>
#include <zephyr/ztest.h>

#define BIOS_FOOD           0xB105F00D
#define SCHED_INVALID       4242
#define INVALID_DETACHSTATE 7373

static bool attr_valid;
static pthread_attr_t attr;
static const pthread_attr_t uninit_attr;
static bool detached_thread_has_finished;

/* TODO: this should be optional */
#define STATIC_THREAD_STACK_SIZE (MAX(1024, PTHREAD_STACK_MIN + CONFIG_TEST_EXTRA_STACK_SIZE))
static K_THREAD_STACK_DEFINE(static_thread_stack, STATIC_THREAD_STACK_SIZE);

static void *thread_entry(void *arg)
{
	bool joinable = (bool)POINTER_TO_UINT(arg);

	if (!joinable) {
		detached_thread_has_finished = true;
	}

	return NULL;
}

static void create_thread_common_entry(const pthread_attr_t *attrp, bool expect_success,
				       bool joinable, void *(*entry)(void *arg), void *arg)
{
	pthread_t th;

	if (!joinable) {
		detached_thread_has_finished = false;
	}

	if (expect_success) {
		zassert_ok(pthread_create(&th, attrp, entry, arg));
	} else {
		zassert_not_ok(pthread_create(&th, attrp, entry, arg));
		return;
	}

	if (joinable) {
		zassert_ok(pthread_join(th, NULL), "failed to join joinable thread");
		return;
	}

	/* should not be able to join detached thread */
	zassert_not_ok(pthread_join(th, NULL));

	for (size_t i = 0; i < 10; ++i) {
		k_msleep(2 * CONFIG_PTHREAD_RECYCLER_DELAY_MS);
		if (detached_thread_has_finished) {
			break;
		}
	}

	zassert_true(detached_thread_has_finished, "detached thread did not seem to finish");
}

static void create_thread_common(const pthread_attr_t *attrp, bool expect_success, bool joinable)
{
	create_thread_common_entry(attrp, expect_success, joinable, thread_entry,
				   UINT_TO_POINTER(joinable));
}

static inline void can_create_thread(const pthread_attr_t *attrp)
{
	create_thread_common(attrp, true, true);
}

static inline void cannot_create_thread(const pthread_attr_t *attrp)
{
	create_thread_common(attrp, false, true);
}

ZTEST(pthread_attr, test_null_attr)
{
	/*
	 * This test can only succeed when it is possible to call pthread_create() with a NULL
	 * pthread_attr_t* (I.e. when we have the ability to allocate thread stacks dynamically).
	 */
	create_thread_common(NULL, IS_ENABLED(CONFIG_DYNAMIC_THREAD) ? true : false, true);
}

ZTEST(pthread_attr, test_pthread_attr_static_corner_cases)
{
	pthread_attr_t attr1;

	Z_TEST_SKIP_IFDEF(CONFIG_DYNAMIC_THREAD);

	/*
	 * These tests are specifically for when dynamic thread stacks are disabled, so passing
	 * a NULL pthread_attr_t* should fail.
	 */
	cannot_create_thread(NULL);

	/*
	 * Additionally, without calling pthread_attr_setstack(), thread creation should fail.
	 */
	zassert_ok(pthread_attr_init(&attr1));
	cannot_create_thread(&attr1);
}

ZTEST(pthread_attr, test_pthread_attr_init_destroy)
{
	/* attr has already been initialized in before() */

	if (false) {
		/* undefined behaviour */
		zassert_ok(pthread_attr_init(&attr));
	}

	/* cannot destroy an uninitialized attr */
	zassert_equal(pthread_attr_destroy((pthread_attr_t *)&uninit_attr), EINVAL);

	can_create_thread(&attr);

	/* can destroy an initialized attr */
	zassert_ok(pthread_attr_destroy(&attr), "failed to destroy an initialized attr");
	attr_valid = false;

	cannot_create_thread(&attr);

	if (false) {
		/* undefined behaviour */
		zassert_ok(pthread_attr_destroy(&attr));
	}

	/* can re-initialize a destroyed attr */
	zassert_ok(pthread_attr_init(&attr));
	/* TODO: pthread_attr_init() should be sufficient to initialize a thread by itself */
	zassert_ok(pthread_attr_setstack(&attr, &static_thread_stack, STATIC_THREAD_STACK_SIZE));
	attr_valid = true;

	can_create_thread(&attr);

	/* note: attr is still valid and is destroyed in after() */
}


ZTEST(pthread_attr, test_pthread_attr_large_stacksize)
{
	size_t actual_size;
	const size_t expect_size = BIT(CONFIG_POSIX_PTHREAD_ATTR_STACKSIZE_BITS);

	if (pthread_attr_setstacksize(&attr, expect_size) != 0) {
		TC_PRINT("Unable to allocate large stack of size %zu (skipping)\n", expect_size);
		ztest_test_skip();
		return;
	}

	zassert_ok(pthread_attr_getstacksize(&attr, &actual_size));
	zassert_equal(actual_size, expect_size);
}

ZTEST(pthread_attr, test_pthread_attr_getdetachstate)
{
	int detachstate;

	/* degenerate cases */
	{
		if (false) {
			/* undefined behaviour */
			zassert_equal(pthread_attr_getdetachstate(NULL, NULL), EINVAL);
			zassert_equal(pthread_attr_getdetachstate(NULL, &detachstate), EINVAL);
			zassert_equal(pthread_attr_getdetachstate(&uninit_attr, &detachstate),
				      EINVAL);
		}
		zassert_equal(pthread_attr_getdetachstate(&attr, NULL), EINVAL);
	}

	/* default detachstate is joinable */
	zassert_ok(pthread_attr_getdetachstate(&attr, &detachstate));
	zassert_equal(detachstate, PTHREAD_CREATE_JOINABLE);
	can_create_thread(&attr);
}

ZTEST(pthread_attr, test_pthread_attr_setdetachstate)
{
	int detachstate = PTHREAD_CREATE_JOINABLE;

	/* degenerate cases */
	{
		if (false) {
			/* undefined behaviour */
			zassert_equal(pthread_attr_setdetachstate(NULL, INVALID_DETACHSTATE),
				      EINVAL);
			zassert_equal(pthread_attr_setdetachstate(NULL, detachstate), EINVAL);
			zassert_equal(pthread_attr_setdetachstate((pthread_attr_t *)&uninit_attr,
								  detachstate),
				      EINVAL);
		}
		zassert_equal(pthread_attr_setdetachstate(&attr, INVALID_DETACHSTATE), EINVAL);
	}

	/* read back detachstate just written */
	zassert_ok(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED));
	zassert_ok(pthread_attr_getdetachstate(&attr, &detachstate));
	zassert_equal(detachstate, PTHREAD_CREATE_DETACHED);
	create_thread_common(&attr, true, false);
}


static void before(void *arg)
{
	ARG_UNUSED(arg);

	zassert_ok(pthread_attr_init(&attr));
	/* TODO: pthread_attr_init() should be sufficient to initialize a thread by itself */
	zassert_ok(pthread_attr_setstack(&attr, &static_thread_stack, STATIC_THREAD_STACK_SIZE));
	attr_valid = true;
}

static void after(void *arg)
{
	ARG_UNUSED(arg);

	if (attr_valid) {
		(void)pthread_attr_destroy(&attr);
		attr_valid = false;
	}
}

ZTEST_SUITE(pthread_attr, NULL, NULL, before, after, NULL);
