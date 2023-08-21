#include <assert.h>
#include <stdio.h>
#include <wayland-server.h>
#include <wlr/backend.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/allocator.h>
#include <wlr/types/wlr_output.h>

// Waylandサーバー
struct morning_server {
    struct wl_display *display;         // wayland display
    struct wlr_backend *backend;        // デバイスとの入出力を抽象化 (DRM, libinput, X11など)
    struct wlr_renderer *renderer;      // レンダリングを抽象化 (Pixman, Vulkan, OpenGLなど)
    struct wlr_allocator *allocator;    // 画面描画のバッファを確保

    struct wl_listener new_output;      // 新しい出力デバイスの接続を検知するリスナー
    struct wl_list outputs;             // 出力デバイス(モニター)のリスト
};

// 新しい出力デバイスが接続された時の関数
static void new_output_notify(struct wl_listener *listener, void *data) {

    // サーバーを取得
    struct morning_server *server = wl_container_of(listener, server, new_output);

    // outputの内容を取得
    struct wlr_output *wlr_output = data;

    // outputのレンダリングサブシステムを初期化
    wlr_output_init_render(wlr_output, server->allocator, server->renderer);

    // outputの状態を取得
	struct wlr_output_state state;
	wlr_output_state_init(&state);

    // outputを有効化
	wlr_output_state_set_enabled(&state, true);

    // outputのmode (解像度やリフレッシュレートなどの情報) を設定
    struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
	if (mode != NULL) {
		wlr_output_state_set_mode(&state, mode);
	}

    // outputの状態をコミット
    // これにより初めて、セッションが画面に適用される
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

    // rendererを作成
    server.renderer = wlr_renderer_autocreate(server.backend);
    assert(server.renderer);
    wlr_renderer_init_wl_display(server.renderer, server.display);

    // allocatorを作成
    server.allocator = wlr_allocator_autocreate(server.backend, server.renderer);
    assert(server.allocator);

    // 出力デバイスのリストを作成
    wl_list_init(&server.outputs);

    // 新規の出力デバイスが接続された時のシグナルを作成
    server.new_output.notify = new_output_notify;
    wl_signal_add(&server.backend->events.new_output, &server.new_output);

    // backendを有効化
    if (!wlr_backend_start(server.backend)) {
        fprintf(stderr, "Failed to start backend\n");
        wlr_backend_destroy(server.backend);
        wl_display_destroy(server.display);
        return 1;
    }

    // セッションを開始
    wl_display_run(server.display);
    wl_display_destroy(server.display);

    return 0;
}