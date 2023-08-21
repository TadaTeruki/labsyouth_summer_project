#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <wayland-server.h>
#include <wlr/backend.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_output.h>

// waylandサーバー
struct morning_server {

    // wayland display
    struct wl_display *display;
    // デバイスとの入出力を抽象化 (DRM, libinput, X11など)
    struct wlr_backend *backend;

    // 新しい入力デバイスの接続を検知するlistener
    struct wl_listener new_input;
    // 新しい出力デバイスの接続を検知するlistener
    struct wl_listener new_output;

    // 認識されたキーボードのリスト
    struct wl_list keyboards;
    // 出力デバイス(モニター)のリスト
    struct wl_list outputs;
};

// キーボードに関するイベントの情報を保持する構造体
struct morning_keyboard {

    // waylandサーバー
    struct morning_server *server;
    // キーボードの情報
    struct wlr_keyboard *wlr_keyboard;

    // キーボードからの入力を検知するlistener
    struct wl_listener keyboard_input;

    // リスト用
    struct wl_list link;
};

// キーボードからの入力を受け取ったときのイベント
static void handle_keyboard_input(struct wl_listener *listener, void *data) {
    printf("detected keyboard input\n");
}

// キーボードの初期化
static void server_new_keyboard(struct morning_server *server, struct wlr_input_device *device) {

    // input_deviceからキーボードの情報を取得
    struct wlr_keyboard *wlr_keyboard = wlr_keyboard_from_input_device(device);

    // XKB keymapを適用
    struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    struct xkb_keymap *keymap = xkb_keymap_new_from_names(context, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS);
    wlr_keyboard_set_keymap(wlr_keyboard, keymap);
    xkb_keymap_unref(keymap);
    xkb_context_unref(context);

    // イベント用のkeyboard構造体を作成
    struct morning_keyboard *keyboard = calloc(1, sizeof(struct morning_keyboard));
    keyboard->server = server;
    keyboard->wlr_keyboard = wlr_keyboard;

    // キーボードのイベントを指定
    keyboard->keyboard_input.notify = handle_keyboard_input;
    wl_signal_add(&wlr_keyboard->events.key, &keyboard->keyboard_input);

    // サーバーのキーボードリストに追加
    wl_list_insert(&server->keyboards, &keyboard->link);
}

// 新しい入力デバイスが接続された時のイベント
static void new_input_notify(struct wl_listener *listener, void *data) {
    struct morning_server *server = wl_container_of(listener, server, new_input);
    struct wlr_input_device *wlr_input_device = data;

    // デバイスの種類に合わせて初期化
    switch (wlr_input_device->type) {
    case WLR_INPUT_DEVICE_KEYBOARD:
        server_new_keyboard(server, wlr_input_device);
        break;
    default:
        break;
    }
}

// 新しい出力デバイスが接続された時のイベント
static void new_output_notify(struct wl_listener *listener, void *data) {

    // サーバーを取得
    struct morning_server *server = wl_container_of(listener, server, new_output);

    // outputのデータを取得
    struct wlr_output *wlr_output = data;

    // outputの状態(state)を格納する構造体を初期化
    struct wlr_output_state state;
    wlr_output_state_init(&state);

    // outputの有効化をstateに適用
    wlr_output_state_set_enabled(&state, true);

    // outputのmode (解像度やリフレッシュレートなどの情報) を設定
    struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
    if (mode != NULL) {
        wlr_output_state_set_mode(&state, mode);
    }

    // outputの状態をコミット
    // これにより初めて、実際のディスプレイ上に何かが映し出される
    wlr_output_commit_state(wlr_output, &state);
    wlr_output_state_finish(&state);
}

int main(int argc, char *argv[]) {

    // サーバをー初期化
    struct morning_server server = {0};

    // wayland displayを作成
    server.display = wl_display_create();
    assert(server.display);

    // backendを作成
    server.backend = wlr_backend_autocreate(server.display, NULL);
    assert(server.backend);

    // 出力デバイスのリストを作成
    wl_list_init(&server.outputs);

    // 新しい出力デバイスが接続された時のイベントを指定
    server.new_output.notify = new_output_notify;
    wl_signal_add(&server.backend->events.new_output, &server.new_output);

    // キーボードのリストを作成
    wl_list_init(&server.keyboards);

    // 新しい入力デバイスが接続された時のイベントを指定
    server.new_input.notify = new_input_notify;
    wl_signal_add(&server.backend->events.new_input, &server.new_input);

    // backendを有効化
    if (!wlr_backend_start(server.backend)) {
        fprintf(stderr, "Failed to start backend\n");
        wlr_backend_destroy(server.backend);
        wl_display_destroy(server.display);
        return 1;
    }

    // 実行
    wl_display_run(server.display);

    // 終了
    wl_display_destroy(server.display);

    return 0;
}