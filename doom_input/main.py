import tkinter as tk
import serial

# --- CONFIGURATION ---
SERIAL_PORT = 'COM3'
BAUD_RATE = 9600

# Mapping: { Key: (Press_Char, Release_Char, Display_Name, Row, Col, Columnspan) }
KEY_MAP = {
    'w':        ('W', 'w', 'W', 0, 1, 1),
    'a':        ('A', 'a', 'A', 1, 0, 1),
    's':        ('S', 's', 'S', 1, 1, 1),
    'd':        ('D', 'd', 'D', 1, 2, 1),
    'e':        ('U', 'u', 'E (Use)', 1, 3, 1),
    'shift_l':  ('R', 'r', 'Shift (Run)', 2, 0, 2),
    'comma':    ('<', ',', '< (Prev)', 2, 2, 1),
    'period':   ('>', '.', '> (Next)', 2, 3, 1),
    'space':    ('F', 'f', 'SPACE (FIRE)', 3, 0, 4),
    'control_l':('C', 'c', 'CTRL', 2, 4, 1),
}

class DoomKeyboardGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("DOOM Keyboard Interface")
        self.root.configure(bg="#121212")

        try:
            self.ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=0)
        except:
            self.ser = None
            print("Warning: Serial port not found.")

        self.buttons = {}
        self.pressed_keys = set()
        
        self.setup_grid_ui()

        self.root.bind("<KeyPress>", self.on_press)
        self.root.bind("<KeyRelease>", self.on_release)

    def setup_grid_ui(self):
        # Create a container frame
        main_frame = tk.Frame(self.root, bg="#121212", padx=20, pady=20)
        main_frame.pack()

        for key, info in KEY_MAP.items():
            # Extract layout info
            char_p, char_r, name, r, c, span = info
            
            btn = tk.Label(
                main_frame, 
                text=name, 
                width=10 * span, # Make wide keys physically wider
                height=2, 
                fg="white", 
                bg="#333333", 
                relief="raised",
                font=("Impact", 10)
            )
            # Use grid instead of pack
            btn.grid(row=r, column=c, columnspan=span, padx=3, pady=3, sticky="nsew")
            self.buttons[key] = btn

    def send_serial(self, char):
        if self.ser:
            self.ser.write(char.encode())

    def on_press(self, event):
        key = event.keysym.lower()
        if key in KEY_MAP and key not in self.pressed_keys:
            self.pressed_keys.add(key)
            self.buttons[key].config(bg="#ff4444", relief="sunken")
            print("sent press:   ", KEY_MAP[key][0])
            self.send_serial(KEY_MAP[key][0])

    def on_release(self, event):
        key = event.keysym.lower()
        if key in KEY_MAP:
            self.pressed_keys.discard(key)
            self.buttons[key].config(bg="#333333", relief="raised")
            print("sent release: ", KEY_MAP[key][1])
            self.send_serial(KEY_MAP[key][1])

if __name__ == "__main__":
    root = tk.Tk()
    app = DoomKeyboardGUI(root)
    root.mainloop()