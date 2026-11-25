/*
 * Copyright 2026 Kerem Söke
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "config.h"

#include <stdlib.h>

#include "libweston-internal.h"
#include "weston-test-client-helper.h"
#include "weston-test-fixture-compositor.h"
#include "weston-test-assert.h"

struct setup_args {
	struct fixture_metadata meta;
	enum weston_renderer_type renderer;
};

static const struct setup_args my_setup_args[] = {
	{
		.renderer = WESTON_RENDERER_PIXMAN,
		.meta.name = "pixman"
	},
	{
		.renderer = WESTON_RENDERER_GL,
		.meta.name = "GL"
	},
	{
		.renderer = WESTON_RENDERER_VULKAN,
		.meta.name = "Vulkan"
	},
};

static enum test_result_code
fixture_setup(struct weston_test_harness *harness, const struct setup_args *arg)
{
	struct compositor_setup setup;

	compositor_setup_defaults(&setup);
	setup.shell = SHELL_TEST_DESKTOP;
	setup.refresh = HIGHEST_OUTPUT_REFRESH;
	setup.renderer = arg->renderer;

	return weston_test_harness_execute_as_client(harness, &setup);
}
DECLARE_FIXTURE_SETUP_WITH_ARG(fixture_setup, my_setup_args, meta);

static enum test_result_code
copy_unpainted_surface(struct wet_testsuite_data *suite_data)
{
	struct client *client;
	struct surface *unpainted;
	struct buffer *unpainted_buf, *buf;
	pixman_color_t red;

	color_rgb888(&red, 255, 0, 0);

	client = create_client_and_test_surface(10, 10, 50, 50);
	test_assert_ptr_not_null(client);

	unpainted = create_test_surface(client);
	weston_test_move_surface(client->test->weston_test,
				 unpainted->wl_surface, 1000, 1000);
	unpainted_buf = surface_commit_color(client, unpainted->wl_surface,
					     &red, 50, 50);

	client_push_breakpoint(client, suite_data,
			       WESTON_TEST_BREAKPOINT_POST_REPAINT,
			       (struct wl_proxy *) client->output->wl_output);
	buf = surface_commit_color(client, client->surface->wl_surface,
				   &red, 50, 50);

	RUN_INSIDE_BREAKPOINT(client, suite_data) {
		struct weston_surface *surface;
		size_t size = 50 * 50 * 4;
		void *pixels = malloc(size);

		test_assert_enum_eq(breakpoint->template_->breakpoint,
				    WESTON_TEST_BREAKPOINT_POST_REPAINT);
		surface = get_resource_data_from_proxy(suite_data,
				(struct wl_proxy *) unpainted->wl_surface);
		test_assert_ptr_not_null(surface->buffer_ref.buffer);
		test_assert_int_eq(weston_surface_copy_content(surface, pixels,
							       size, 0, 0,
							       50, 50), -1);
		free(pixels);
	}

	buffer_destroy(buf);
	buffer_destroy(unpainted_buf);
	surface_destroy(unpainted);
	client_destroy(client);

	return RESULT_OK;
}

DECLARE_TEST_LIST(
	TESTFN(copy_unpainted_surface),
);
