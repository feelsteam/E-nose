# ============================================================================
# app_v3.py — E-Nose 伺服器 v3（Flask + TCP Socket + Gemini AI）
# Raspberry Pi 版本
# ============================================================================

from flask import Flask, request, jsonify, send_from_directory
from flask_cors import CORS
import mysql.connector
import threading
import socket
import uuid
import time
import os
from datetime import datetime
from google import genai

# ============================================================================
# 設定
# ============================================================================
MYSQL_HOST = '127.0.0.1'
MYSQL_USER = 'root'
MYSQL_PASS = 'smellgood2026'
MYSQL_DB   = 'smellgood'

FLASK_PORT  = 5000
DEVICE_PORT = 9000

STATIC_DIR = os.path.normpath(os.path.join(os.path.dirname(__file__), 'static'))

GEMINI_API_KEY = 'AIzaSyDPdUUcRRVsTsvqr2MwrNZkNa3dRzcu43I'

# ============================================================================
# Flask
# ============================================================================
app = Flask(__name__, static_folder=STATIC_DIR, static_url_path='')
CORS(app, resources={r"/api/*": {"origins": "*"}})

# ============================================================================
# 全域狀態
# ============================================================================
device_socket    = None
device_lock      = threading.Lock()
device_connected = False

pending_tasks   = {}
completed_tasks = {}
partial_results = {}

# ============================================================================
# DB 操作
# ============================================================================
def get_db():
    return mysql.connector.connect(
        host=MYSQL_HOST, user=MYSQL_USER,
        password=MYSQL_PASS, database=MYSQL_DB
    )

def get_prev_value(subject_id):
    try:
        db = get_db(); c = db.cursor()
        c.execute(
            "SELECT acetone_ppm, uploaded_at FROM `value` "
            "WHERE subject_id=%s ORDER BY uploaded_at DESC LIMIT 1",
            (subject_id,)
        )
        row = c.fetchone()
        c.close(); db.close()
        return row
    except Exception as e:
        print(f"[DB] get_prev_value error: {e}")
        return None

def insert_value(subject_id, ppm, meal_state):
    try:
        db = get_db(); c = db.cursor()
        c.execute(
            "INSERT INTO `value` (subject_id, acetone_ppm, meal_state) VALUES (%s,%s,%s)",
            (subject_id, ppm, meal_state)
        )
        db.commit(); c.close(); db.close()
        print(f"[DB] value 已儲存: {subject_id} {ppm:.3f}ppm [{meal_state}]")
    except Exception as e:
        print(f"[DB] insert_value error: {e}")

def insert_ans(subject_id, pred, conf, amax, slope):
    try:
        db = get_db(); c = db.cursor()
        c.execute(
            "INSERT INTO `ans` (subject_id, prediction, confidence, acetone_max, acetone_slope) "
            "VALUES (%s,%s,%s,%s,%s)",
            (subject_id, pred, conf, amax, slope)
        )
        db.commit(); c.close(); db.close()
        print(f"[DB] ans 已儲存: {subject_id} pred={pred} conf={conf:.3f}")
    except Exception as e:
        print(f"[DB] insert_ans error: {e}")

# ============================================================================
# TCP Server（與 BMduino 裝置通訊）
# ============================================================================
def tcp_server():
    global device_socket, device_connected
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(('0.0.0.0', DEVICE_PORT))
    srv.listen(1)
    print(f"[TCP] 監聽 port {DEVICE_PORT}...")
    while True:
        try:
            client, addr = srv.accept()
            print(f"[TCP] BMduino 連線：{addr}")
            with device_lock:
                if device_socket:
                    try: device_socket.close()
                    except: pass
                device_socket = client
                device_connected = True
            handle_device(client)
        except Exception as e:
            print(f"[TCP] Accept error: {e}")
            time.sleep(1)

def handle_device(client):
    global device_socket, device_connected
    buf = ""
    client.settimeout(None)
    while True:
        try:
            data = client.recv(1024)
            if not data: break
            buf += data.decode('utf-8', errors='ignore')
            while '\n' in buf:
                line, buf = buf.split('\n', 1)
                line = line.strip()
                if line: on_device_message(line, client)
        except ConnectionResetError: break
        except Exception as e:
            print(f"[TCP] Read error: {e}"); break
    with device_lock:
        device_socket = None
        device_connected = False
    print("[TCP] BMduino 已斷線")

def send_to_device(client, msg):
    try:
        client.sendall((msg + '\n').encode('utf-8'))
        print(f"[→ Device] {msg}")
    except Exception as e:
        print(f"[TCP] Send error: {e}")

def on_device_message(msg, client):
    print(f"[← Device] {msg}")
    parts = msg.split('|')
    msgtype = parts[0]

    if msgtype == 'HELLO':
        print(f"[Device] {parts[1] if len(parts)>1 else '?'}")
        return

    if msgtype == 'POLL':
        now = time.time()
        for k in [k for k,v in pending_tasks.items() if now-v['created_at']>600]:
            del pending_tasks[k]

        if not pending_tasks:
            send_to_device(client, "WAIT")
            return

        tid = min(pending_tasks, key=lambda k: pending_tasks[k]['created_at'])
        t   = pending_tasks[tid]
        g   = 1 if t['gender']=='Male' else 0

        if t['mode'] == 'ready':
            resp = f"READY|{tid}|{g}|{t['age']}|{t['bmi']:.1f}"
        else:
            resp = f"COUNT|{tid}|{g}|{t['age']}|{t['bmi']:.1f}|{t['prev_ppm']:.3f}|{t['time_diff_min']:.1f}"

        send_to_device(client, resp)
        print(f"[POLL] 派發任務 {tid} mode={t['mode']}")
        return

    if msgtype == 'MEASURE' and len(parts) >= 4:
        tid        = parts[1]
        ppm        = float(parts[2])
        meal_state = parts[3]

        task = pending_tasks.get(tid)
        subject_id = task['subject_id'] if task else tid
        task_mode  = task['mode'] if task else 'ready'

        insert_value(subject_id, ppm, meal_state)

        if task_mode == 'ready':
            meta = pending_tasks.pop(tid, {'created_at': time.time()})
            completed_tasks[tid] = {
                'status':     'done',
                'type':       'simple',
                'created_at': meta['created_at'],
                'result': {
                    'acetone_ppm': ppm,
                    'meal_state':  meal_state,
                    'message':     '空腹丙酮已儲存，請在飯後2小時再次測量。'
                }
            }
            print(f"[MEASURE] READY 完成，ppm={ppm:.3f}")
        else:
            partial_results[tid] = {'ppm': ppm, 'meal_state': meal_state}
            print(f"[MEASURE] COUNT partial，ppm={ppm:.3f}，等待 RESULT...")
        return

    if msgtype == 'RESULT' and len(parts) >= 6:
        tid   = parts[1]
        pred  = int(parts[2])
        conf  = float(parts[3])
        amax  = float(parts[4])
        slope = float(parts[5])

        task = pending_tasks.get(tid)
        subject_id = task['subject_id'] if task else tid
        prev_ppm   = task['prev_ppm']   if task else 0.0
        user_age   = task['age']        if task else 22
        user_bmi   = task['bmi']        if task else 24.0

        partial    = partial_results.pop(tid, {})
        curr_ppm   = partial.get('ppm', 0.0)
        meal_state = partial.get('meal_state', 'postmeal_2h')

        insert_ans(subject_id, pred, conf, amax, slope)

        print(f"[AI] 正在呼叫 Gemini 分析 {subject_id} 的數據...")
        ai_advice_text = "AI 建議暫時無法使用，量測資料已正常儲存。"
        if GEMINI_API_KEY != '請替換成你的_API_KEY':
            ai_client = genai.Client(api_key=GEMINI_API_KEY)
            prompt = f"""
            你是一位溫暖的代謝健康教練，來自 Feels Team。
            請根據以下數據提供個人化分析：
            【基本資料】年齡 {user_age} 歲、BMI {user_bmi:.1f}。
            【量測特徵】丙酮最大值 {amax} ppm、動態斜率 {slope}。
            【系統判斷】風險代碼 {pred} (0=健康, 1=風險)。

            任務指令：
            1. 綜合評估：結合特徵與風險代碼，用白話文解釋目前的代謝狀況。
            2. 具體建議：給予具體的運動或飲食注意事項。
            3. 嚴格守則：不可給出醫療診斷，語氣要像朋友般鼓勵。字數約 120 字。
            """
            delay = 3
            for attempt in range(3):
                try:
                    ai_response = ai_client.models.generate_content(model='gemini-2.5-flash', contents=prompt)
                    ai_advice_text = ai_response.text
                    print(f"[AI] 建議生成成功（第 {attempt+1} 次）")
                    break
                except Exception as e:
                    if attempt < 2:
                        print(f"[AI] 第 {attempt+1} 次失敗，{delay}s 後重試: {e}")
                        time.sleep(delay)
                        delay *= 2
                    else:
                        print(f"[AI] 呼叫失敗（已重試 3 次）: {e}")
        else:
            ai_advice_text = "請先在後端程式碼中設定 GEMINI_API_KEY。"

        meta = pending_tasks.pop(tid, {'created_at': time.time()})
        completed_tasks[tid] = {
            'status':     'done',
            'type':       'detailed',
            'created_at': meta['created_at'],
            'result': {
                'prediction':     pred,
                'confidence':     conf,
                'acetone_max':    amax,
                'acetone_slope':  slope,
                'prev_ppm':       prev_ppm,
                'curr_ppm':       curr_ppm,
                'meal_state':     meal_state,
                'ai_advice':      ai_advice_text
            }
        }
        print(f"[RESULT] COUNT 完成，pred={pred} conf={conf:.3f}")
        return


# ============================================================================
# Flask 路由
# ============================================================================
@app.route('/')
def index():
    return send_from_directory(STATIC_DIR, 'index.html')

@app.route('/api/status')
def api_status():
    return jsonify({
        'server':       'online',
        'device':       'connected' if device_connected else 'disconnected',
        'pendingCount': len(pending_tasks)
    })

@app.route('/api/start', methods=['POST'])
def api_start():
    data       = request.json
    subject_id = data.get('subjectId','').strip()
    gender     = data.get('gender','Male')
    age        = int(data.get('age', 0))
    height     = float(data.get('height', 0))
    weight     = float(data.get('weight', 0))
    meal_state = data.get('mealState','fasting')

    if not subject_id or age<=0 or height<=0 or weight<=0:
        return jsonify({'status':'error','message':'請填寫所有必要欄位'}), 400

    bmi = round(weight / ((height/100)**2), 2)

    try:
        db = get_db(); c = db.cursor()
        c.execute("""
            INSERT INTO `human` (subject_id, gender, age, bmi)
            VALUES (%s,%s,%s,%s)
            ON DUPLICATE KEY UPDATE gender=VALUES(gender), age=VALUES(age), bmi=VALUES(bmi)
        """, (subject_id, gender, age, bmi))
        db.commit(); c.close(); db.close()
    except Exception as e:
        return jsonify({'status':'error','message':f'DB error: {e}'}), 500

    prev = get_prev_value(subject_id)

    task_id = str(uuid.uuid4())[:8]
    task    = {
        'subject_id': subject_id,
        'gender':     gender,
        'age':        age,
        'bmi':        bmi,
        'meal_state': meal_state,
        'created_at': time.time(),
    }

    if prev is None:
        task['mode'] = 'ready'
        msg_mode = 'first'
    else:
        prev_ppm, prev_time = prev[0], prev[1]
        if isinstance(prev_time, str):
            prev_time = datetime.fromisoformat(prev_time)
        diff_min = (datetime.now() - prev_time).total_seconds() / 60.0
        task['mode']          = 'count'
        task['prev_ppm']      = prev_ppm
        task['time_diff_min'] = diff_min
        msg_mode = f'second (prev={prev_ppm:.3f}ppm, {diff_min:.0f}min ago)'

    pending_tasks[task_id] = task
    print(f"[API] 任務 {task_id} 建立，mode={task['mode']} {msg_mode}")

    return jsonify({
        'status':  'success',
        'taskId':  task_id,
        'bmi':     bmi,
        'mode':    task['mode'],
        'message': '等待 BMduino 採集...'
    })

@app.route('/api/result/<task_id>')
def api_result(task_id):
    if task_id in completed_tasks:
        t = completed_tasks[task_id]
        if time.time() - t['created_at'] > 600:
            t['status'] = 'timeout'
        return jsonify({'status': t['status'], 'type': t.get('type'), 'result': t.get('result')})

    if task_id in pending_tasks:
        return jsonify({'status':'waiting','result':None,'deviceStatus':'waiting_for_device'})

    return jsonify({'status':'error','message':'Task not found'}), 404


# ============================================================================
# 主程式
# ============================================================================
if __name__ == '__main__':
    print(f"[INFO] Static: {STATIC_DIR}")
    threading.Thread(target=tcp_server, daemon=True).start()
    print("="*50)
    print("  SmellGood E-Nose v3 Server (Raspberry Pi)")
    print(f"  HTTP: http://0.0.0.0:{FLASK_PORT}")
    print(f"  TCP:  port {DEVICE_PORT}")
    print("="*50)
    app.run(host='0.0.0.0', port=FLASK_PORT, debug=False, use_reloader=False)
