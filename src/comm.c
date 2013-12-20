#include <pebble.h>
#include "comm.h"
#include "misc.h"
#include "consts.h"
#include "tasklists.h"
#include "tasks.h"
#include "statusbar.h"

static bool comm_js_ready = false;
static CommJsReadyCallback comm_js_ready_cb;
static void *comm_js_ready_cb_data;
static bool comm_unsent_message = false; // if some message is waiting

static void comm_send_if_js_ready() {
	if(comm_js_ready) {
		LOG("JS is ready, sending");
		app_message_outbox_send();
	} else if(!comm_unsent_message) {
		LOG("JS is not ready, planning");
		comm_unsent_message = true; // will be cleared on js_ready
	} else {
		APP_LOG(APP_LOG_LEVEL_ERROR, "!!! Tried to send message to JS part but there is already a message waiting");
		sb_show("Internal error: already sending msg");
	}
}

void comm_query_tasklists() {
	LOG("Querying tasklists");
	DictionaryIterator *iter;
	Tuplet code = TupletInteger(KEY_CODE, CODE_GET);
	Tuplet scope = TupletInteger(KEY_SCOPE, SCOPE_LISTS);

	app_message_outbox_begin(&iter);
	dict_write_tuplet(iter, &code);
	dict_write_tuplet(iter, &scope);
	comm_send_if_js_ready();
}
void comm_query_tasks(int listId) {
	LOG("Querying tasks for listId=%d", listId);
	DictionaryIterator *iter;
	Tuplet code = TupletInteger(KEY_CODE, CODE_GET);
	Tuplet scope = TupletInteger(KEY_SCOPE, SCOPE_TASKS);
	Tuplet tListId = TupletInteger(KEY_LISTID, listId);

	app_message_outbox_begin(&iter);
	dict_write_tuplet(iter, &code);
	dict_write_tuplet(iter, &scope);
	dict_write_tuplet(iter, &tListId);
	comm_send_if_js_ready();
}
void comm_query_task_details(int listId, int taskId) {
	LOG("Querying task details for %d, %d (not implemented)", listId, taskId);
}

bool comm_is_busy() {
	return comm_unsent_message;
}

static void comm_in_received_handler(DictionaryIterator *iter, void *context) {
	Tuple *tCode, *tMessage, *tScope;

	tCode = dict_find(iter, KEY_CODE);
	assert(tCode, "Message without code");
	int code = (int)tCode->value->int32;
	LOG("Message code: %d", code);

	if(code == CODE_ERROR) {
		tMessage = dict_find(iter, KEY_ERROR);
		char* message = "Unknown error";
		if(tMessage && tMessage->type == TUPLE_CSTRING)
			message = tMessage->value->cstring;
		LOG("Error received: %s", message);
		sb_show(message);
		return;
	} else if(code == CODE_READY) { // JS just loaded
		comm_js_ready = true;
		if(comm_unsent_message) {
			LOG("Have unsent message, sending");
			app_message_outbox_send();
			comm_unsent_message = false;
		}
		if(comm_js_ready_cb) {
			LOG("JS Ready Callback awaiting, calling");
			comm_js_ready_cb(comm_js_ready_cb_data);
		}
		return;
	}

	tScope = dict_find(iter, KEY_SCOPE);
	assert(tScope, "No scope!");
	int scope = (int)tScope->value->int32;
	LOG("Message scope: %d", scope);

	if(scope == SCOPE_LISTS) {
		if(!tl_is_active()) { // lists window is not active; ignoring message
			LOG("Ignoring TaskLists-related message because they are inactive");
			return;
		}
		if(code == CODE_ARRAY_START) {
			int count = (int)dict_find(iter, KEY_COUNT)->value->int32;
			LOG("Items count: %d", count);
			tl_set_count(count);
		} else if(code == CODE_ARRAY_ITEM) {
			int i = (int)dict_find(iter, KEY_ITEM)->value->int32;
			int listId = (int)dict_find(iter, KEY_LISTID)->value->int32;
			char *title = dict_find(iter, KEY_TITLE)->value->cstring;
			int size = (int)dict_find(iter, KEY_SIZE)->value->int32;
			LOG("Item No: %d, Id=%d, size=%d", i, listId, size);
			tl_set_item(i, (TL_Item){
				.id = listId,
				.title = title,
				.size = size,
			});
		} else if(code == CODE_ARRAY_END) {
			sb_hide(); // hide load percentage
		} else
			LOG("Unexpected message code: %d", code);
	} else if(scope == SCOPE_TASKS) {
		assert(ts_is_active(), "Ignoring Tasks-related message because they are inactive");
		// TODO: ignore tasks for not-current listid
		if(code == CODE_ARRAY_START) {
			int count = (int)dict_find(iter, KEY_COUNT)->value->int32;
			int listId = (int)dict_find(iter, KEY_LISTID)->value->int32;
			if(listId != ts_current_listId())
				sb_show("Warning: list ID mismatch");
			LOG("Items count: %d", count);
			ts_set_count(count);
		} else if(code == CODE_ARRAY_ITEM) {
			int i = (int)dict_find(iter, KEY_ITEM)->value->int32;
			int taskId = (int)dict_find(iter, KEY_TASKID)->value->int32;
			char *title = dict_find(iter, KEY_TITLE)->value->cstring;
			Tuple *tNotes = dict_find(iter, KEY_NOTES);
			char *notes = NULL;
			if(tNotes)
				notes = tNotes->value->cstring;
			bool isDone = (bool)dict_find(iter, KEY_ISDONE)->value->int32;
			LOG("Item No: %d, Id=%d, done=%d", i, taskId, isDone);
			ts_set_item(i, (TS_Item){
				.id = taskId,
				.done = isDone,
				.title = title,
				.notes = notes,
			});
		} else if(code == CODE_ARRAY_END) {
			sb_hide(); // hide load percentage
		} else
			LOG("Unexpected message code: %d", code);
	} else {
		LOG("Unexpected scope: %d", scope);
	}
}
static void comm_in_dropped_handler(AppMessageResult reason, void *context) {
	LOG("Message dropped: reason=%d", reason);
}
static void comm_out_sent_handler(DictionaryIterator *sent, void *context) {
	LOG("Message sent");
}
static void comm_out_failed_handler(DictionaryIterator *failed, AppMessageResult reason, void *context) {
	LOG("Message send failed: reason=%d", reason);
}
void comm_init() {
	app_message_register_inbox_received(comm_in_received_handler);
	app_message_register_inbox_dropped(comm_in_dropped_handler);
	app_message_register_outbox_sent(comm_out_sent_handler);
	app_message_register_outbox_failed(comm_out_failed_handler);

	app_message_open(app_message_inbox_size_maximum(), APP_MESSAGE_OUTBOX_SIZE_MINIMUM); // We only need large buffer for inbox
}
void comm_deinit() {
	app_message_deregister_callbacks();
}
