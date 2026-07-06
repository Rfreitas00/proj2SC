import sensor, image, time, ml, bluetooth
from micropython import const

sensor.reset()
sensor.set_pixformat(sensor.GRAYSCALE)
sensor.set_framesize(sensor.QVGA)
sensor.set_windowing((240, 240))
sensor.skip_frames(time=2000)

model  = ml.Model("tflite_learn_993688_14.tflite", load_to_fb=True)
labels = ["Blank", "O", "X"]

GRID_SIZE      = 3
CONFIRM_FRAMES = 8
FRAME_HISTORY  = 15

_SERVICE_UUID = bluetooth.UUID("12345678-1234-1234-1234-123456789abc")
_CMD_UUID     = bluetooth.UUID("12345678-1234-1234-1234-123456789a01")
_ACK_UUID     = bluetooth.UUID("12345678-1234-1234-1234-123456789a02")
_FLAG_READ    = const(0x0002)
_FLAG_WRITE   = const(0x0008)
_FLAG_NOTIFY  = const(0x0010)
_IRQ_CENTRAL_CONNECT    = const(1)
_IRQ_CENTRAL_DISCONNECT = const(2)
_IRQ_GATTS_WRITE        = const(3)

ble = bluetooth.BLE()
ble.active(True)

_service = (
    _SERVICE_UUID, (
        (_CMD_UUID, _FLAG_READ | _FLAG_NOTIFY),
        (_ACK_UUID, _FLAG_WRITE),
    ),
)
((h_cmd, h_ack),) = ble.gatts_register_services((_service,))

central_connected = False
ack_received      = False
conn_handle       = None   

def _irq(event, data):
    global central_connected, ack_received, conn_handle
    if event == _IRQ_CENTRAL_CONNECT:
        conn_handle       = data[0]  
        central_connected = True
        print("[BLE] Nano ligado, handle:", conn_handle)
        time.sleep_ms(500)
    elif event == _IRQ_CENTRAL_DISCONNECT:
        central_connected = False
        conn_handle       = None
        print("[BLE] Nano desligado — a re-anunciar...")
        _advertise()
    elif event == _IRQ_GATTS_WRITE:
        _, value_handle = data
        if value_handle == h_ack:
            val = ble.gatts_read(h_ack).decode().strip()
            if val == "OK":
                ack_received = True
                print("[BLE] ACK recebido")

ble.irq(_irq)

def _advertise(interval_us=200_000):
    name = b"TTT-Nicla"
    payload = (b"\x02\x01\x06"
               + bytes([len(name) + 1, 0x09])
               + name)
    ble.gap_advertise(interval_us, adv_data=payload)
    print("[BLE] A anunciar como TTT-Nicla...")

_advertise()

def ble_send(msg_str, wait_ack=True, timeout_ms=10000):
    global ack_received, conn_handle
    if not central_connected or conn_handle is None:
        print("[BLE] Nano não ligado — comando ignorado.")
        return False
    try:
        ble.gatts_notify(conn_handle, h_cmd, msg_str.encode())
        print(f"[BLE] Enviado: {msg_str.strip()}")
    except OSError as e:
        print(f"[BLE] Erro ao enviar: {e}")
        return False
    if not wait_ack:
        return True
    ack_received = False
    t0 = time.ticks_ms()
    while not ack_received:
        if time.ticks_diff(time.ticks_ms(), t0) > timeout_ms:
            print("[BLE] Timeout ACK!")
            return False
        time.sleep_ms(20)
    return True

def detect_grid(img):
    return len(img.find_lines(threshold=1000, theta_margin=25, rho_margin=25)) >= 4

def get_majority_vote(history):
    if not history:
        return None
    result = []
    for cell in range(9):
        votes = {}
        for g in history:
            votes[g[cell]] = votes.get(g[cell], 0) + 1
        result.append(max(votes, key=votes.get))
    return result

def check_winner(grid):
    for combo in [[0,1,2],[3,4,5],[6,7,8],
                  [0,3,6],[1,4,7],[2,5,8],
                  [0,4,8],[2,4,6]]:
        vals = [grid[i] for i in combo]
        if vals[0] != "Blank" and vals[0] == vals[1] == vals[2]:
            return vals[0]
    return None

def print_grid(grid):
    print("\n+---+---+---+")
    for row in range(3):
        line = "| "
        for col in range(3):
            s = " " if grid[row*3+col] == "Blank" else grid[row*3+col]
            line += f"{s} | "
        print(line)
        print("+---+---+---+")

def choose_move(grid):
    wins = [[0,1,2],[3,4,5],[6,7,8],
            [0,3,6],[1,4,7],[2,5,8],
            [0,4,8],[2,4,6]]
    for combo in wins:
        xs     = [i for i in combo if grid[i] == "X"]
        blanks = [i for i in combo if grid[i] == "Blank"]
        if len(xs) == 2 and len(blanks) == 1:
            return blanks[0]
    for combo in wins:
        os     = [i for i in combo if grid[i] == "O"]
        blanks = [i for i in combo if grid[i] == "Blank"]
        if len(os) == 2 and len(blanks) == 1:
            return blanks[0]
    if grid[4] == "Blank":
        return 4
    for c in [0, 2, 6, 8]:
        if grid[c] == "Blank":
            return c
    for i in range(9):
        if grid[i] == "Blank":
            return i
    return None

def apply_robot_cells(camera_grid, robot_cells):
    merged = []
    for i in range(9):
        if i in robot_cells:
            merged.append("X")
        elif camera_grid[i] == "O":
            merged.append("O")
        else:
            merged.append("Blank")
    return merged

grid_counter    = 0
no_grid_counter = 0
grid_visible    = False
last_grid       = None
history_grids   = []
robot_cells     = []
game_over       = False
waiting_for_ack = False

clock = time.clock()

print("=== TTT Nicla Vision + BLE ===")
print("Humano: O  |  Robô: X  |  Humano começa")
print("Aguarda ligação BLE do Nano...")

while True:
    clock.tick()
    img = sensor.snapshot()

    # ── Detecção de grelha ──
    if detect_grid(img):
        grid_counter    += 1
        no_grid_counter  = 0
    else:
        no_grid_counter += 1
        grid_counter     = 0

    if grid_counter >= CONFIRM_FRAMES:
        grid_visible = True
    elif no_grid_counter >= CONFIRM_FRAMES:
        if grid_visible:
            print("\n--- Grelha perdida — jogo reiniciado ---")
        grid_visible    = False
        last_grid       = None
        history_grids   = []
        robot_cells     = []
        game_over       = False
        waiting_for_ack = False

    if not grid_visible:
        img.draw_string(2, 2, "Aguarda grelha...")
        continue

    if game_over:
        img.draw_string(2, 2, "FIM DE JOGO")
        continue

    if waiting_for_ack:
        img.draw_string(2, 2, "Robo a desenhar...")
        continue

    w, h   = img.width(), img.height()
    cw, ch = w // GRID_SIZE, h // GRID_SIZE

    current_grid_camera = []
    for row in range(GRID_SIZE):
        for col in range(GRID_SIZE):
            x, y     = col * cw, row * ch
            cell_img = img.copy(roi=(x, y, cw, ch))
            preds    = model.predict([cell_img])[0].flatten().tolist()
            best_idx = preds.index(max(preds))
            current_grid_camera.append(labels[best_idx])
            s = " " if labels[best_idx] == "Blank" else labels[best_idx]
            img.draw_rectangle(x, y, cw, ch)
            img.draw_string(x + 2, y + 2, s)

    history_grids.append(current_grid_camera)
    if len(history_grids) > FRAME_HISTORY:
        history_grids.pop(0)

    stable_camera = get_majority_vote(history_grids)
    if not stable_camera:
        continue

    stable_grid = apply_robot_cells(stable_camera, robot_cells)

    if stable_grid == last_grid:
        continue

    last_grid = stable_grid[:]
    print_grid(stable_grid)

    winner = check_winner(stable_grid)
    if winner:
        print(f">>> VENCEDOR: {winner}! <<<")
        ble_send(f"WIN:{winner}\n", wait_ack=False)
        game_over = True
        continue

    if "Blank" not in stable_grid:
        print(">>> EMPATE! <<<")
        ble_send("DRAW\n", wait_ack=False)
        game_over = True
        continue

    num_o = stable_grid.count("O")
    num_x = len(robot_cells)

    if num_o > num_x:
        move = choose_move(stable_grid)
        if move is not None:
            robot_cells.append(move)
            print(f">>> Robô joga célula {move} <<<")
            waiting_for_ack = True
            ok = ble_send(f"DRAW X {move}\n", wait_ack=True)
            waiting_for_ack = False
            if not ok:
                robot_cells.pop()
                print("[BLE] Falhou — vai tentar na próxima frame")
