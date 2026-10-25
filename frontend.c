#define _POSIX_C_SOURCE 200809L
#define CLAY_IMPLEMENTATION
#include "clay.h"
#include "clay_renderer_raylib.c"
#include "backend.h"

#include <sys/wait.h>
#include <unistd.h>

extern int generate_puzzle(uint8_t *board);

enum { WINDOW_WIDTH = 480, WINDOW_HEIGHT = 564, CELL_SIZE = 44, BOARD_MARGIN = 26 };
static const Color PAPER = {243, 231, 204, 255};
static const Color CELL_BG = {255, 247, 229, 255};
static const Color INK = {32, 60, 74, 255};
static const Color TEAL = {40, 98, 104, 255};
static const Color COMPLETE = {196, 213, 188, 255};
static const Color OCHRE = {180, 121, 33, 255};
static const Color CONFLICT = {230, 175, 148, 255};

typedef struct App {
	game_state board;
	bool givens[9][9];
	int row, col;
	bool vim, busy;
	char status[256];
	char boxes[96];
} App;

static Clay_Color clay_color(Color color) {
	return (Clay_Color){color.r, color.g, color.b, color.a};
}

static Clay_String clay_string(const char *text) {
	return (Clay_String){.length = (int32_t)strlen(text), .chars = text};
}

static void clay_error(Clay_ErrorData error) {
	fprintf(stderr, "Clay: %.*s\n", error.errorText.length, error.errorText.chars);
}

static void new_puzzle(App *app) {
	app->busy = true;
	app->status[0] = '\0';
}

// Called only after the "Generating..." frame has been presented.
static void finish_generation(App *app) {
	app->board = (game_state){0};
	generate_puzzle(&app->board.nums[0][0]);
	game_loop(&app->board);
	for (int r = 0; r < 9; r++) {
		for (int c = 0; c < 9; c++) app->givens[r][c] = app->board.nums[r][c] != 0;
	}
	app->busy = false;
}

/* Commands are fixed strings, never constructed from filenames or user input. */
static bool choose_csv(App *app, bool save, char *path, size_t capacity) {
	const char *command = save
		? "zenity --file-selection --save --title='Save CSV' --filename=board.csv "
			"--file-filter='CSV boards | *.csv' --file-filter='All files | *'"
		: "zenity --file-selection --title='Load CSV' "
			"--file-filter='CSV boards | *.csv' --file-filter='All files | *'";
	FILE *dialog = popen(command, "r");
	if (!dialog) {
		snprintf(app->status, sizeof app->status, "Could not open file dialog (install Zenity).");
		return false;
	}
	size_t length = fread(path, 1, capacity - 1, dialog);
	bool too_long = length == capacity - 1;
	bool read_error = ferror(dialog);
	// Drain output before waiting for the process, even for an overlong path.
	while (fgetc(dialog) != EOF) {}
	int result = pclose(dialog);
	if (result == -1 || !WIFEXITED(result) || WEXITSTATUS(result) > 1) {
		snprintf(app->status, sizeof app->status, "File dialog failed; install Zenity and check your display.");
		return false;
	}
	if (WEXITSTATUS(result) == 1) return false; // Cancel.
	if (too_long || read_error) {
		snprintf(app->status, sizeof app->status, "Could not read the selected path.");
		return false;
	}
	if (length && path[length - 1] == '\n') length--;
	path[length] = '\0';
	if (!length) return false;
	if (save) {
		const char *name = strrchr(path, '/');
		name = name ? name + 1 : path;
		if (!strchr(name, '.')) {
			if (length + sizeof ".csv" > capacity) return false;
			memcpy(path + length, ".csv", sizeof ".csv");
		}
		if (access(path, F_OK) == 0 && system(
			"zenity --question --title='Overwrite file?' "
			"--text='The selected file already exists. Replace it?' "
			"--ok-label=Replace --cancel-label=Cancel") != 0) return false;
	}
	// Modal dialogs must not leave their keystrokes queued as board edits.
	while (GetCharPressed()) {}
	while (GetKeyPressed()) {}
	return true;
}

static void load_csv(App *app) {
	char path[4096];
	if (!choose_csv(app, false, path, sizeof path)) return;
	game_state candidate = {0};
	if (read_from_csv(&candidate, path) != 0) {
		snprintf(app->status, sizeof app->status, "Could not load the CSV board.");
		return;
	}
	game_loop(&candidate);
	app->board = candidate;
	memset(app->givens, 0, sizeof app->givens);
	app->status[0] = '\0';
}

static void save_csv(App *app) {
	char path[4096];
	if (!choose_csv(app, true, path, sizeof path)) return;
	if (write_to_csv(&app->board, path) != 0) {
		snprintf(app->status, sizeof app->status, "Could not save the CSV board.");
	} else {
		app->status[0] = '\0';
	}
}

static void button(Clay_ElementId id, const char *label, bool disabled, bool selected) {
	Color color = selected ? OCHRE : TEAL;
	if (!disabled && Clay_PointerOver(id)) color = INK;
	CLAY(id, {
		.layout = {
			.sizing = {.width = CLAY_SIZING_FIXED(104), .height = CLAY_SIZING_FIXED(34)},
			.childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER}
		},
		.backgroundColor = clay_color(color)
	}) {
		CLAY_TEXT(clay_string(label), CLAY_TEXT_CONFIG({
			.textColor = clay_color(disabled ? COMPLETE : CELL_BG), .fontSize = 18
		}));
	}
}

static Clay_RenderCommandArray layout(App *app) {
	bool won = app->board.has_won;
	snprintf(app->boxes, sizeof app->boxes, "Completed boxes: ");
	bool any = false;
	for (int i = 0; i < 9; i++) {
		if (won || app->board.box_is_right[i]) {
			size_t n = strlen(app->boxes);
			snprintf(app->boxes + n, sizeof app->boxes - n, "%s%d", any ? ", " : "", i + 1);
			any = true;
		}
	}
	if (!any) strcat(app->boxes, "none");
	const char *status = app->busy ? "Generating..." : app->status[0]
		? app->status : won ? "Solved!" : "";
	Clay_BeginLayout();
	CLAY(CLAY_ID("Root"), {
		.layout = {
			.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()},
			.padding = CLAY_PADDING_ALL(12), .childGap = 6,
			.childAlignment.x = CLAY_ALIGN_X_CENTER, .layoutDirection = CLAY_TOP_TO_BOTTOM
		}
	}) {
		CLAY(CLAY_ID("Toolbar"), {.layout = {.childGap = 8}}) {
			button(CLAY_ID("New"), "New puzzle", app->busy, false);
			button(CLAY_ID("Load"), "Load CSV", app->busy, false);
			button(CLAY_ID("Save"), "Save CSV", app->busy, false);
			button(CLAY_ID("Vim"), app->vim ? "[x] Vim keys" : "[ ] Vim keys", false, app->vim);
		}
		// Clay positions the canvas; raylib draws the grid and its contents below.
		CLAY(CLAY_ID("Board"), {.layout.sizing = {
			.width = CLAY_SIZING_FIXED(448), .height = CLAY_SIZING_FIXED(448)
		}}) {}
		CLAY_TEXT(clay_string(app->boxes), CLAY_TEXT_CONFIG({
			.textColor = clay_color(TEAL), .fontSize = 18
		}));
		CLAY_TEXT(clay_string(status), CLAY_TEXT_CONFIG({
			.textColor = clay_color(INK), .fontSize = 18
		}));
	}
	return Clay_EndLayout(GetFrameTime());
}

static Rectangle board_rectangle(void) {
	Clay_BoundingBox b = Clay_GetElementData(CLAY_ID("Board")).boundingBox;
	return (Rectangle){b.x + BOARD_MARGIN, b.y + BOARD_MARGIN, CELL_SIZE * 9, CELL_SIZE * 9};
}

static void move(App *app, int dr, int dc) {
	app->row = (app->row + dr + 9) % 9;
	app->col = (app->col + dc + 9) % 9;
}

static void edit(App *app, int value) {
	if (app->givens[app->row][app->col]) return;
	app->board.nums[app->row][app->col] = (uint8_t)value;
	game_loop(&app->board);
	app->status[0] = '\0';
}

static bool pressed(int key) {
	return IsKeyPressed(key) || IsKeyPressedRepeat(key);
}

static void input(App *app) {
	if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
		Vector2 mouse = GetMousePosition();
		Rectangle board = board_rectangle();
		if (CheckCollisionPointRec(mouse, board)) {
			app->row = (int)((mouse.y - board.y) / CELL_SIZE);
			app->col = (int)((mouse.x - board.x) / CELL_SIZE);
		}
		if (Clay_PointerOver(CLAY_ID("Vim"))) app->vim = !app->vim;
		if (!app->busy) {
			if (Clay_PointerOver(CLAY_ID("New"))) { new_puzzle(app); return; }
			if (Clay_PointerOver(CLAY_ID("Load"))) { load_csv(app); return; }
			if (Clay_PointerOver(CLAY_ID("Save"))) { save_csv(app); return; }
		}
	}
	if (app->busy) {
		while (GetCharPressed()) {}
		return;
	}
	if (pressed(KEY_UP)) move(app, -1, 0);
	if (pressed(KEY_DOWN)) move(app, 1, 0);
	if (pressed(KEY_LEFT)) move(app, 0, -1);
	if (pressed(KEY_RIGHT)) move(app, 0, 1);
	if (pressed(KEY_BACKSPACE) || pressed(KEY_DELETE)) edit(app, 0);
	for (int ch; (ch = GetCharPressed()) != 0;) {
		if (ch >= '0' && ch <= '9') edit(app, ch - '0');
		if (!app->vim) continue;
		switch (ch) {
			case 'h': move(app, 0, -1); break;
			case 'j': move(app, 1, 0); break;
			case 'k': move(app, -1, 0); break;
			case 'l': move(app, 0, 1); break;
			case 'x': edit(app, 0); break;
		}
	}
}

static void checkmark(float x, float y) {
	DrawLineEx((Vector2){x - 5, y}, (Vector2){x - 1, y + 4}, 2, TEAL);
	DrawLineEx((Vector2){x - 1, y + 4}, (Vector2){x + 6, y - 5}, 2, TEAL);
}

static void draw_board(App *app, Font font) {
	Rectangle board = board_rectangle();
	bool won = app->board.has_won;
	for (int r = 0; r < 9; r++) {
		for (int c = 0; c < 9; c++) {
			Color color = CELL_BG;
			if (won) color = COMPLETE;
			else if (app->board.state[r][c]) color = CONFLICT;
			else if (app->board.row_is_right[r] || app->board.column_is_right[c]
				|| app->board.box_is_right[r / 3 * 3 + c / 3]) color = COMPLETE;
			Rectangle cell = {board.x + c * CELL_SIZE, board.y + r * CELL_SIZE, CELL_SIZE, CELL_SIZE};
			DrawRectangleRec(cell, color);
			if (app->board.nums[r][c]) {
				char digit[] = {(char)('0' + app->board.nums[r][c]), '\0'};
				Vector2 extent = MeasureTextEx(font, digit, 26, 0);
				Vector2 pos = {cell.x + (CELL_SIZE - extent.x) / 2, cell.y + (CELL_SIZE - extent.y) / 2};
				bool given = app->givens[r][c];
				DrawTextEx(font, digit, pos, 26, 0, given ? INK : TEAL);
				if (given) {
					pos.x += 0.7f;
					DrawTextEx(font, digit, pos, 26, 0, INK);
				}
			}
		}
	}
	for (int i = 0; i <= 9; i++) {
		float x = board.x + i * CELL_SIZE, y = board.y + i * CELL_SIZE;
		float width = i % 3 == 0 ? 3 : 1;
		DrawLineEx((Vector2){board.x, y}, (Vector2){board.x + board.width, y}, width, INK);
		DrawLineEx((Vector2){x, board.y}, (Vector2){x, board.y + board.height}, width, INK);
	}
	DrawRectangleLinesEx((Rectangle){board.x + app->col * CELL_SIZE + 3,
		board.y + app->row * CELL_SIZE + 3, CELL_SIZE - 6, CELL_SIZE - 6}, 3, OCHRE);
	for (int i = 0; i < 9; i++) {
		if (won || app->board.column_is_right[i]) checkmark(board.x + (i + .5f) * CELL_SIZE, board.y - 13);
		if (won || app->board.row_is_right[i]) checkmark(board.x + board.width + 13, board.y + (i + .5f) * CELL_SIZE);
	}
}

int main(void) {
	App app = {0};
	Clay_Raylib_Initialize(WINDOW_WIDTH, WINDOW_HEIGHT, "Just Sudoku", FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
	SetExitKey(KEY_NULL);
	SetTargetFPS(60);
	Font fonts[] = {GetFontDefault()};
	size_t capacity = Clay_MinMemorySize();
	void *memory = malloc(capacity);
	if (!memory) {
		fprintf(stderr, "Could not allocate Clay's layout arena.\n");
		Clay_Raylib_Close();
		return 1;
	}
	Clay_Initialize(Clay_CreateArenaWithCapacityAndMemory(capacity, memory),
		(Clay_Dimensions){WINDOW_WIDTH, WINDOW_HEIGHT}, (Clay_ErrorHandler){clay_error, NULL});
	Clay_SetMeasureTextFunction(Raylib_MeasureText, fonts);
	new_puzzle(&app);
	while (!WindowShouldClose()) {
		Vector2 mouse = GetMousePosition();
		Clay_SetPointerState((Clay_Vector2){mouse.x, mouse.y}, IsMouseButtonDown(MOUSE_BUTTON_LEFT));
		Clay_SetLayoutDimensions((Clay_Dimensions){GetScreenWidth(), GetScreenHeight()});
		Clay_RenderCommandArray commands = layout(&app);
		input(&app);
		if (app.busy) commands = layout(&app);
		BeginDrawing();
		ClearBackground(PAPER);
		Clay_Raylib_Render(commands, fonts);
		draw_board(&app, fonts[0]);
		EndDrawing();
		if (app.busy) finish_generation(&app);
	}
	UnloadShader(overlayShader);
	Clay_Raylib_Close();
	free(memory);
	return 0;
}
