#include "../runtime/tui.c"
#include <assert.h>

static unsigned random_state = 1729;
static unsigned next_random(void) {
    random_state = random_state * 1664525u + 1013904223u;
    return random_state;
}
static void gap_random(void) {
    TcGap *gap = (TcGap *)tc_gap_create(TC_STRING(""));
    char model[12000];
    size_t length = 0, cursor = 0;
    int i;
    for (i = 0; i < 10000; i++) {
        unsigned operation = next_random() % 4;
        if (operation == 0) {
            char value = (char)('a' + next_random() % 26);
            TinyString input = {&value, 1};
            assert(tc_gap_insert(gap, input) == 0);
            memmove(model + cursor + 1, model + cursor, length - cursor);
            model[cursor++] = value;
            length++;
        } else if (operation == 1 && cursor) {
            tc_gap_backspace(gap);
            memmove(model + cursor - 1, model + cursor, length - cursor);
            cursor--;
            length--;
        } else if (operation == 2 && cursor < length) {
            tc_gap_delete(gap);
            memmove(model + cursor, model + cursor + 1, length - cursor - 1);
            length--;
        } else {
            cursor = next_random() % (length + 1);
            tc_gap_seek(gap, cursor);
        }
        {
            TinyString text = tc_gap_text(gap);
            assert(text.length == length && memcmp(text.data, model, length) == 0);
            assert(tc_gap_position(gap) == cursor);
            tc_string_free(text);
        }
    }
    tc_gap_destroy(gap);
}
static void gap_unicode(void) {
    TcGap *g = (TcGap *)tc_gap_create(TC_STRING("a\303\261\346\227\245\nsecond\n"));
    TinyString cut, text;
    tc_gap_right(g);
    tc_gap_right(g);
    assert(tc_gap_position(g) == 3);
    tc_gap_backspace(g);
    assert(tc_gap_position(g) == 1);
    tc_gap_delete(g);
    text = tc_gap_text(g);
    assert(tc_string_equal(text, TC_STRING("a\nsecond\n")));
    tc_string_free(text);
    tc_gap_goto_line(g, 1);
    assert(tc_gap_line(g) == 1);
    tc_gap_end(g);
    assert(tc_gap_column(g) == 6);
    cut = tc_gap_cut_line(g);
    assert(tc_string_equal(cut, TC_STRING("second\n")));
    assert(tc_gap_insert(g, cut) == 0);
    tc_string_free(cut);
    assert(tc_gap_find(g, TC_STRING("second"), 0) == 2);
    tc_gap_destroy(g);
}
static void increment(void *opaque) {
    (*(int *)opaque)++;
}
int main(void) {
    TcUi *ui;
    int error = 0, clicks = 0, i;
    TinyString ansi, snapshot;
    TcWidget *column, *label, *textbox, *row, *button;
    TcUiClosure callback;
    gap_random();
    gap_unicode();
    ui = (TcUi *)tc_ui_create(30, 8, 1, &error);
    assert(ui && error == 0);
    tc_ui_clear(ui, 0xffffff, 0);
    ansi = tc_ui_render(ui);
    assert(tc_ui_changed(ui) == 240);
    tc_string_free(ansi);
    ansi = tc_ui_render(ui);
    assert(!ansi.length && !tc_ui_changed(ui));
    tc_string_free(ansi);
    tc_ui_put(ui, 2, 3, 'X', 0xffffff, 0);
    ansi = tc_ui_render(ui);
    assert(tc_ui_changed(ui) == 1 && strstr(ansi.data, "\033[4;3H"));
    tc_string_free(ansi);
    assert(tc_ui_resize(ui, 0, 5) != 0);
    column = (TcWidget *)tc_widget_create(TC_WIDGET_COLUMN, TC_STRING(""), 0, 0);
    label = (TcWidget *)tc_widget_create(TC_WIDGET_LABEL, TC_STRING("TinyUI test"), 0, 1);
    textbox = (TcWidget *)tc_widget_create(TC_WIDGET_TEXTBOX, TC_STRING("text"), 0, 0);
    row = (TcWidget *)tc_widget_create(TC_WIDGET_ROW, TC_STRING(""), 0, 1);
    button = (TcWidget *)tc_widget_create(TC_WIDGET_BUTTON, TC_STRING("Save"), 0, 1);
    assert(tc_widget_add(column, label) == 0 && tc_widget_add(column, textbox) == 0 &&
           tc_widget_add(column, row) == 0 && tc_widget_add(row, button) == 0);
    assert(tc_widget_add(row, column) != 0 && tc_widget_add(row, label) != 0);
    callback.environment = &clicks;
    callback.invoke = increment;
    callback.owned = false;
    tc_widget_on_click(button, &callback);
    tc_ui_content(ui, column);
    tc_ui_draw(ui);
    assert(textbox->height == 6 && row->y == 7);
    snapshot = tc_ui_snapshot(ui);
    assert(strstr(snapshot.data, "TinyUI test") && strstr(snapshot.data, "[ Save ]"));
    tc_string_free(snapshot);
    tc_ui_focus(ui, textbox);
    tc_ui_dispatch(ui, 1, 0, 0, 0, 0, 0, 0x65e5);
    assert(tc_gap_position(textbox->buffer) == 3);
    tc_ui_dispatch(ui, 1, 9, 0, 0, 0, 0, 9);
    assert(ui->focus == button);
    tc_ui_dispatch(ui, 1, 13, 0, 0, 0, 0, 13);
    assert(clicks == 1);
    tc_ui_dispatch(ui, 4, 1, button->x, button->y, 0, 0, 0);
    assert(clicks == 2);
    for (i = 0; i < 64; i++)
        assert(tc_ui_post(ui, 5, i, 0, 0, 0) == 0);
    assert(tc_ui_post(ui, 5, 0, 0, 0, 0) == 8);
    for (i = 0; i < 64; i++) {
        TcUiEvent event = tc_ui_event(ui, 0);
        assert(event.kind == 5 && event.key == i);
    }
    assert(tc_ui_event(ui, 0).kind == 3);
    tc_ui_destroy(ui);
    puts("TUI runtime verified: gap fuzz, UTF-8 edits, layout, cell diffs, focus, callbacks, queue "
         "bounds");
    return 0;
}
