#include <pebble.h>
#include "task.h"
#include "tasks.h"
#include "comm.h"
#include "misc.h"
#include "statusbar.h"
#include "options.h"

static Window *wndTask;
static ScrollLayer *slTask;
static TextLayer *tlTaskTitle;
static TextLayer *tlTaskNotes;
static GBitmap *bmpTasks[2];

static int listId = -1;
static int taskId = -1;
static bool done;
static char* title;
static char* notes;

static void t_window_load(Window *wnd) {
	Layer *wnd_layer = window_get_root_layer(wnd);
	GRect bounds = layer_get_bounds(wnd_layer);

	slTasks = menu_layer_create(bounds);
	menu_layer_set_callbacks(mlTasks, NULL, (MenuLayerCallbacks) {
		.get_num_rows = ts_get_num_rows_cb,
		.get_header_height = ts_get_header_height_cb,
		.draw_header = ts_draw_header_cb,
		.draw_row = ts_draw_row_cb,
		.select_click = ts_select_click_cb,
	});
	menu_layer_set_click_config_onto_window(mlTasks, wnd);
	layer_add_child(wnd_layer, menu_layer_get_layer(mlTasks));
}
static void t_window_unload(Window *wnd) {
	menu_layer_destroy(mlTasks);
}

/* Public functions */

void t_init() {
	wndTask = window_create();
	window_set_window_handlers(wndTask, (WindowHandlers) {
		.load = t_window_load,
		.disappear = sb_window_disappear_cb,
		.unload = t_window_unload,
	});
	LOG("Task module initialized, window is %p", wndTasks);
}
void t_deinit() {
	window_destroy(wndTask);
}
void t_show(int vlistId, int vtaskId, TS_Item item) {
	LOG("Showing task for listId=%d and taskId=%d", vlistId, vtaskId);
	listId = vlistId;
	taskId = vtaskId;
	done = item.done;
	title = item.title;
	notes = item.notes;

	window_stack_push(wndTask, true);
}
int t_current_listId() {
	return listId;
}
int t_current_taskId() {
	return taskId;
}
