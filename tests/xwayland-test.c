/*
 * Copyright © 2015 Samsung Electronics Co., Ltd
 * Copyright 2022 Collabora, Ltd.
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
 *
 * xwayland-test: Confirm that we can map a window and we're running
 *		  under Xwayland, not just X.
 *
 *		  This is done in steps:
 *		  1) Confirm that the WL_SURFACE_ID atom exists
 *		  2) Confirm that our window name is "Xwayland Test Window"
 *		  3) Confirm that there's conforming Window Manager
 *		  4) Confirm that the window manager's name is "Weston WM"
 *		  5) Make sure we can map a window
 */

#include "config.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "weston-test-runner.h"
#include "weston-test-fixture-compositor.h"
#include "shared/string-helpers.h"
#include "weston-test-client-helper.h"
#include "weston-test-assert.h"
#include "xcb-client-helper.h"

static enum test_result_code
fixture_setup(struct weston_test_harness *harness)
{
	struct compositor_setup setup;

	unsetenv("DISPLAY");

	compositor_setup_defaults(&setup);
	setup.shell = SHELL_TEST_DESKTOP;
	setup.refresh = HIGHEST_OUTPUT_REFRESH;
	setup.xwayland = true;
	setup.logging_scopes = "log,xwm-wm-x11";

	return weston_test_harness_execute_as_client(harness, &setup);
}
DECLARE_FIXTURE_SETUP(fixture_setup);

static char *
get_x11_window_name(struct window_x11 *window, xcb_drawable_t win)
{
	xcb_get_property_reply_t *reply;
	int reply_len;
	char *name;
	struct atom_x11 *atoms = window_get_atoms(window);

	reply = window_x11_dump_prop(window, win, atoms->net_wm_name);
	test_assert_ptr_not_null(reply);

	test_assert_true(reply->type == atoms->string ||
			 reply->type == atoms->utf8_string);
	reply_len = xcb_get_property_value_length(reply);
	test_assert_int_gt(reply_len, 0);

	str_printf(&name, "%.*s", reply_len,
		   (char *) xcb_get_property_value(reply));
	free(reply);
	return name;
}

static char *
get_wm_name(struct window_x11 *window)
{
	xcb_get_property_reply_t *reply;
	xcb_generic_error_t *error;
	xcb_get_property_cookie_t prop_cookie;
	char *wm_name = NULL;
	struct atom_x11 *atoms = window_get_atoms(window);
	struct xcb_connection_t *conn = window_get_connection(window);

	prop_cookie = xcb_get_property(conn, 0, window->root_win_id,
				       atoms->net_supporting_wm_check,
				       XCB_ATOM_WINDOW, 0, 1024);
	reply = xcb_get_property_reply(conn, prop_cookie, &error);
	test_assert_ptr_not_null(reply);
	test_assert_enum_eq(reply->type, XCB_ATOM_WINDOW);
	test_assert_u8_eq(reply->format, 32);

	xcb_window_t wm_id = *(xcb_window_t *) xcb_get_property_value(reply);
	wm_name = get_x11_window_name(window, wm_id);
	free(reply);

	free(error);
	return wm_name;
}

static enum test_result_code
xwayland_client_test(struct wet_testsuite_data *suite_data)
{
	struct window_x11 *window;
	struct connection_x11 *conn;
	xcb_get_property_reply_t *reply;
	char *win_name;
	char *wm_name;
	pixman_color_t bg_color;
	struct atom_x11 *atoms;

	color_rgb888(&bg_color, 255, 0, 0);

	test_assert_ptr_not_null(getenv("DISPLAY"));

	conn = create_x11_connection();
	test_assert_ptr_not_null(conn);
	window = create_x11_window(100, 100, 100, 100, conn, bg_color, NULL);
	test_assert_ptr_not_null(window);

	window_x11_set_win_name(window, "Xwayland Test Window");
	handle_events_and_check_flags(window, PROPERTY_NAME);


	/* The Window Manager MUST set _NET_SUPPORTING_WM_CHECK on the root
	 * window to be the ID of a child window created by himself, to
	 * indicate that a compliant window manager is active.
	 *
	 * The child window MUST also have the _NET_SUPPORTING_WM_CHECK
	 * property set to the ID of the child window. The child window MUST
	 * also have the _NET_WM_NAME property set to the name of the Window
	 * Manager.
	 *
	 * See Extended Window Manager Hints,
	 * https://specifications.freedesktop.org/wm-spec/latest/ar01s03.html,
	 * _NET_SUPPORTING_WM_CHECK
	 * */
	atoms = window_get_atoms(window);
	test_assert_false(atoms->net_supporting_wm_check == XCB_ATOM_NONE);
	test_assert_false(atoms->wl_surface_id == XCB_ATOM_NONE);
	test_assert_false(atoms->net_wm_name == XCB_ATOM_NONE);
	test_assert_false(atoms->utf8_string == XCB_ATOM_NONE);

	reply = window_x11_dump_prop(window, window->root_win_id,
				     atoms->net_supporting_wm_check);
	test_assert_ptr_not_null(reply);
	test_assert_u8_eq(reply->type, XCB_ATOM_WINDOW);
	free(reply);

	window_x11_map(window);
	handle_events_and_check_flags(window, MAPPED);

	win_name = get_x11_window_name(window, window->win_id);
	test_assert_str_eq(win_name, "Xwayland Test Window");
	free(win_name);

	wm_name = get_wm_name(window);
	test_assert_ptr_not_null(wm_name);
	test_assert_str_eq(wm_name, "Weston WM");
	free(wm_name);

	window_x11_unmap(window);
	handle_events_and_check_flags(window, UNMAPPED);

	destroy_x11_window(window);
	destroy_x11_connection(conn);

	return RESULT_OK;
}

static enum test_result_code
xwayland_id_reuse_test(struct wet_testsuite_data *suite_data)
{
	struct window_x11 *window;
	struct connection_x11 *conn;
	pixman_color_t bg_color;
	xcb_connection_t *xcb_conn;
	xcb_generic_event_t *event;
	xcb_void_cookie_t cookie;
	uint32_t event_mask = XCB_EVENT_MASK_STRUCTURE_NOTIFY;
	bool frame_destroyed;

	color_rgb888(&bg_color, 255, 0, 0);
	conn = create_x11_connection();
	test_assert_ptr_not_null(conn);
	window = create_x11_window(100, 100, 100, 100, conn, bg_color, NULL);
	test_assert_ptr_not_null(window);

	window_x11_map(window);
	handle_events_and_check_flags(window, MAPPED);

	window_x11_unmap(window);
	handle_events_and_check_flags(window, UNMAPPED);

	/* Destroy the window and create a new one with the same window-id */
	xcb_conn = window_get_connection(window);
	xcb_change_window_attributes(xcb_conn, window->frame_id,
				     XCB_CW_EVENT_MASK, &event_mask);
	xcb_grab_server(xcb_conn);
	xcb_destroy_window(xcb_conn, window->win_id);
	cookie = xcb_create_window_checked(xcb_conn, XCB_COPY_FROM_PARENT,
					   window->win_id, window->root_win_id,
					   100, 100, 100, 100, 0,
					   XCB_WINDOW_CLASS_INPUT_OUTPUT,
					   window->screen->root_visual,
					   XCB_CW_EVENT_MASK, &event_mask);
	test_assert_ptr_null(xcb_request_check(xcb_conn, cookie));
	xcb_ungrab_server(xcb_conn);
	xcb_flush(xcb_conn);

	/* Verify that we don't get any reparent events until the destroy
	 * event arrives.
	 */
	do {
		event = xcb_wait_for_event(xcb_conn);
		test_assert_ptr_not_null(event);
		if (!test_assert_u8_ne(EVENT_TYPE(event), XCB_REPARENT_NOTIFY)) {
			free(event);
			break;
		}
		frame_destroyed = EVENT_TYPE(event) == XCB_DESTROY_NOTIFY &&
			((xcb_destroy_notify_event_t *)event)->window ==
			window->frame_id;
		free(event);
	} while (!frame_destroyed);

	destroy_x11_window(window);
	destroy_x11_connection(conn);

	return RESULT_OK;
}

DECLARE_TEST_LIST(
	TESTFN(xwayland_client_test),
	TESTFN(xwayland_id_reuse_test),
);
