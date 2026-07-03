/*
 * Copyright © 2025 Collabora, Ltd.
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

#include "color-representation-common.h"

#include "image-iter.h"
#include "pixel-formats.h"
#include "shared/weston-drm-fourcc.h"
#include "shared/xalloc.h"
#include "weston-test-client-helper.h"
#include "weston-test-assert.h"
#include "xdg-client-helper.h"

static const struct setup_args my_setup_args[] = {
	{
		.meta.name = "GL - Legacy properties",
		.renderer = WESTON_RENDERER_GL,
		.buffer_type = CLIENT_BUFFER_TYPE_DMABUF,
	},
	{
		.meta.name = "GL - Plane color pipeline",
		.renderer = WESTON_RENDERER_GL,
		.buffer_type = CLIENT_BUFFER_TYPE_DMABUF,
		.required_capabilities = WESTON_CAP_PLANE_COLOR_PIPELINE,
		.drm_force_plane_color_pipeline = true,
	},
	{
		.meta.name = "Vulkan - Legacy properties",
		.renderer = WESTON_RENDERER_VULKAN,
		.buffer_type = CLIENT_BUFFER_TYPE_DMABUF,
	},
	{
		.meta.name = "Vulkan - Plane color pipeline",
		.renderer = WESTON_RENDERER_VULKAN,
		.buffer_type = CLIENT_BUFFER_TYPE_DMABUF,
		.required_capabilities = WESTON_CAP_PLANE_COLOR_PIPELINE,
		.drm_force_plane_color_pipeline = true,
	},
};

static enum test_result_code
fixture_setup(struct weston_test_harness *harness, const struct setup_args *arg)
{
	struct compositor_setup setup;

	compositor_setup_defaults(&setup);
	setup.backend = WESTON_BACKEND_DRM;
	setup.renderer = arg->renderer;
	setup.logging_scopes = "log,drm-backend";

	/* Currently enforced by vkms. Set as a reminder for the future. */
	setup.width = 1024;
	setup.height = 768;

	setup.test_quirks.required_capabilities =
		WESTON_CAP_COLOR_REP | arg->required_capabilities;
	setup.test_quirks.drm_force_plane_color_pipeline =
		arg->drm_force_plane_color_pipeline;
	setup.test_quirks.gl_force_import_yuv_fallback =
		arg->gl_force_import_yuv_fallback;

	return weston_test_harness_execute_as_client(harness, &setup);
}
DECLARE_FIXTURE_SETUP_WITH_ARG(fixture_setup, my_setup_args, meta);

static enum test_result_code
color_representation_drm(struct wet_testsuite_data *suite_data,
			 const struct color_state *color_state)
{
	const struct setup_args *args = &my_setup_args[get_test_fixture_index()];
	enum cr_workaround workarounds = 0;

	if (args->required_capabilities & WESTON_CAP_PLANE_COLOR_PIPELINE)
		workarounds |= CR_WORKAROUND_RESET_COLOR_PIPELINE;

	return test_color_representation(color_state, args->buffer_type,
					 CLIENT_BUFFER_TYPE_DMABUF,
					 WESTON_CAPTURE_V1_SOURCE_WRITEBACK,
					 FB_PRESENTED_ZERO_COPY,
					 workarounds);
}

DECLARE_TEST_LIST(
	TESTFN_ARG(color_representation_drm, color_state_cases),
);
