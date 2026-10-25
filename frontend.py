# vim: set noexpandtab tabstop=2 shiftwidth=2 softtabstop=2:
import ctypes as C
import os
from pathlib import Path
import tkinter as tk
from tkinter import filedialog, messagebox


class GameState(C.Structure):
	_fields_ = [
		("nums", (C.c_uint8 * 9) * 9),
		("state", (C.c_uint8 * 9) * 9),
		("row_is_right", C.c_bool * 9),
		("column_is_right", C.c_bool * 9),
		("box_is_right", C.c_bool * 9),
		("has_won", C.c_bool),
	]


def main():
	directory = Path(__file__).resolve().parent / "bin"
	try:
		native = C.CDLL(str(directory / "libsudoku.so"))
		native.game_loop.argtypes = [C.POINTER(GameState)]
		native.game_loop.restype = C.POINTER(GameState)
		for name in ("read_from_csv", "write_to_csv"):
			function = getattr(native, name)
			function.argtypes = [C.POINTER(GameState), C.c_char_p]
			function.restype = C.c_int
	except (OSError, AttributeError) as error:
		raise SystemExit(f"Build bin/libsudoku.so first (commands at top of frontend.py): {error}") from error

	try:
		generator = C.CDLL(str(directory / "libgenerator.so"))
		generate = generator.generate_puzzle
		generate.argtypes = [C.POINTER(C.c_uint8)]
		generate.restype = C.c_int
		generator_error = None
	except (OSError, AttributeError) as error:
		generate = None
		generator_error = str(error)

	class App:
		CELL, OFFSET = 44, 26
		PAPER = "#F3E7CC"
		CELL_BG = "#FFF7E5"
		INK = "#203C4A"
		TEAL = "#286268"
		COMPLETE = "#C4D5BC"
		OCHRE = "#B47921"
		CONFLICT = "#E6AF94"

		def __init__(self, root):
			self.root = root
			self.board = GameState()
			self.givens = set()
			self.selected = (0, 0)
			self.busy = False
			root.title("Just Sudoku")
			root.resizable(False, False)
			root.configure(bg=self.PAPER)
			bar = tk.Frame(root, bg=self.PAPER)
			bar.pack(pady=8)
			self.buttons = []
			for text, command in (("New puzzle", self.new), ("Load CSV", self.load), ("Save CSV", self.save)):
				button = tk.Button(bar, text=text, command=command,
					bg=self.TEAL, fg=self.CELL_BG, activebackground=self.INK,
					activeforeground=self.CELL_BG, disabledforeground=self.COMPLETE,
					relief="flat", padx=12, pady=6, cursor="hand2")
				button.pack(side="left", padx=5)
				self.buttons.append(button)
			self.vim_mode = tk.BooleanVar(value=False)
			tk.Checkbutton(bar, text="Vim keys", variable=self.vim_mode,
				command=lambda: self.canvas.focus_set(), bg=self.PAPER, fg=self.INK,
				activebackground=self.PAPER, activeforeground=self.INK,
				selectcolor=self.CELL_BG, highlightthickness=0).pack(side="left", padx=5)
			extent = self.OFFSET * 2 + self.CELL * 9
			self.canvas = tk.Canvas(root, width=extent, height=extent, bg=self.PAPER, highlightthickness=0)
			self.canvas.pack(padx=8)
			self.canvas.bind("<Button-1>", self.click)
			self.canvas.bind("<Key>", self.key)
			self.boxes = tk.StringVar()
			tk.Label(root, textvariable=self.boxes, bg=self.PAPER, fg=self.TEAL).pack()
			self.status = tk.StringVar()
			tk.Label(root, textvariable=self.status, bg=self.PAPER, fg=self.INK,
				font=("TkDefaultFont", 11, "bold")).pack(pady=(2, 8))
			self.draw()
			self.canvas.focus_set()
			if generate is not None:
				root.after(0, self.new)

		def draw(self):
			canvas, size, offset = self.canvas, self.CELL, self.OFFSET
			canvas.delete("all")
			won = self.board.has_won
			for row in range(9):
				for col in range(9):
					if won:
						color = self.COMPLETE
					elif self.board.state[row][col]:
						color = self.CONFLICT
					else:
						complete = (self.board.row_is_right[row] or self.board.column_is_right[col]
							or self.board.box_is_right[row // 3 * 3 + col // 3])
						color = self.COMPLETE if complete else self.CELL_BG
					x, y = offset + col * size, offset + row * size
					canvas.create_rectangle(x, y, x + size, y + size, fill=color, outline="")
					value = self.board.nums[row][col]
					if value:
						given = (row, col) in self.givens
						canvas.create_text(x + size / 2, y + size / 2, text=str(value),
							font=("TkDefaultFont", 19, "bold" if given else "normal"),
							fill=self.INK if given else self.TEAL)
			for i in range(10):
				p = offset + i * size
				width = 3 if i % 3 == 0 else 1
				canvas.create_line(offset, p, offset + 9 * size, p, width=width, fill=self.INK)
				canvas.create_line(p, offset, p, offset + 9 * size, width=width, fill=self.INK)
			row, col = self.selected
			x, y = offset + col * size, offset + row * size
			canvas.create_rectangle(x + 3, y + 3, x + size - 3, y + size - 3, outline=self.OCHRE, width=3)
			for i in range(9):
				if won or self.board.column_is_right[i]:
					canvas.create_text(offset + (i + .5) * size, offset / 2, text="✓", fill=self.TEAL)
				if won or self.board.row_is_right[i]:
					canvas.create_text(offset + 9 * size + offset / 2, offset + (i + .5) * size,
						text="✓", fill=self.TEAL)
			boxes = [str(i + 1) for i in range(9) if won or self.board.box_is_right[i]]
			self.boxes.set("Completed boxes: " + (", ".join(boxes) or "—"))
			self.status.set("Generating…" if self.busy else "Solved!" if won else "")

		def click(self, event):
			row, col = (event.y - self.OFFSET) // self.CELL, (event.x - self.OFFSET) // self.CELL
			if 0 <= row < 9 and 0 <= col < 9:
				self.selected = (row, col)
				self.canvas.focus_set()
				self.draw()

		def key(self, event):
			if self.busy:
				return "break"
			row, col = self.selected
			moves = {"Up": (-1, 0), "Down": (1, 0), "Left": (0, -1), "Right": (0, 1)}
			vim = self.vim_mode.get()
			if vim:
				moves.update({"h": (0, -1), "j": (1, 0), "k": (-1, 0), "l": (0, 1)})
			if event.keysym in moves:
				dr, dc = moves[event.keysym]
				self.selected = ((row + dr) % 9, (col + dc) % 9)
			elif (row, col) not in self.givens:
				if len(event.char) == 1 and event.char in "123456789":
					self.board.nums[row][col] = int(event.char)
				elif (event.keysym in ("BackSpace", "Delete") or event.char == "0"
					or (vim and event.char == "x")):
					self.board.nums[row][col] = 0
				else:
					return
				native.game_loop(C.byref(self.board))
			self.draw()
			return "break"

		def new(self):
			if self.busy:
				return
			if generate is None:
				messagebox.showerror("Go initializer unavailable", f"{generator_error}\n\n"
					"Use //export generate_puzzle (no space after //) in Go,\n"
					"then build bin/libgenerator.so. See frontend.py for commands.")
				return
			self.busy = True
			for button in self.buttons:
				button.config(state="disabled")
			self.draw()
			# Present "Generating…" before blocking on the native initializer.
			self.root.update_idletasks()
			try:
				board = GameState()
				generate(C.cast(board.nums, C.POINTER(C.c_uint8)))
				native.game_loop(C.byref(board))
				self.board = board
				self.givens = {(r, c) for r in range(9) for c in range(9) if board.nums[r][c]}
			except Exception as error:
				messagebox.showerror("Generation failed", str(error))
			finally:
				self.busy = False
				for button in self.buttons:
					button.config(state="normal")
				self.draw()
				self.canvas.focus_set()

		def load(self):
			path = filedialog.askopenfilename(filetypes=[("CSV boards", "*.csv"), ("All files", "*")])
			if not path:
				return
			board = GameState()
			if native.read_from_csv(C.byref(board), os.fsencode(path)) != 0:
				messagebox.showerror("Load failed", "Could not load the CSV board.")
				return
			native.game_loop(C.byref(board))
			self.board, self.givens = board, set()
			self.draw()
			self.canvas.focus_set()

		def save(self):
			path = filedialog.asksaveasfilename(defaultextension=".csv", filetypes=[("CSV boards", "*.csv")])
			if path and native.write_to_csv(C.byref(self.board), os.fsencode(path)) != 0:
				messagebox.showerror("Save failed", "Could not save the CSV board.")
			self.canvas.focus_set()

	try:
		root = tk.Tk()
	except tk.TclError as error:
		raise SystemExit(f"Cannot open graphical display: {error}") from error
	App(root)
	root.mainloop()

if __name__ == "__main__":
	main()
