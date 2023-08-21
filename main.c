#include <assert.h>
#include <stdio.h>
#include <wayland-server.h>
#include <wlr/backend.h>
#include <wlr/types/wlr_output.h>

// waylandサーバー
struct morning_server {
    struct wl_display *display;         // wayland display
    struct wlr_backend *backend;        // デバイスとの入出力を抽象化 (DRM, libinput, X11など)

    struct wl_listener new_output;      // 新しい出力デバイスの接続を検知するリスナー
    struct wl_list outputs;             // 出力デバイス(モニター)のリスト
};

// 新しい出力デバイスが接続された時の実行内容
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

    // 新しい出力デバイスが接続された時の実行内容を指定
    server.new_output.notify = new_output_notify;
    wl_signal_add(&server.backend->events.new_output, &server.new_output);

    // backendを有効化
    if (!wlr_backend_start(server.backend)) {
        fprintf(stderr, "Failed to start backend\n");
        wlr_backend_destroy(server.backend);
        wl_display_destroy(server.display);
        return 1;
    }

    // 実行
    wl_display_run(server.display);
    wl_display_destroy(server.display);

    return 0;
}