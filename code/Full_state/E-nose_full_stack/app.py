# ============================================================================
# app.py — E-Nose Full Stack Server (v2 被動配對架構)
# ============================================================================
# 架構：
#   BMduino 主動量測 → 完成後發 OFFER → 伺服器配對待處理請求
#   - 有待處理請求且有前次紀錄 → PREDICT (BMduino 執行二階段預測)
#   - 有待處理請求但無前次紀錄 → ACCEPT (伺服器直接寫入)
#   - 沒有待處理請求 → REJECT (捨棄)
#
# 啟動方式：py app.py
# ============================================================================

from flask import Flask, request, jsonify, send_from_directory
from flask_cors import CORS
import mysql.connector
import threading
import socket
import uuid
import time
from datetime import datetime

# ============================================================================
# 設定區（請依實際環境修改）
# ============================================================================
MYSQL_HOST  = '127.0.0.1'
MYSQL_USER  = 'root'
MYSQL_PASS  = '00000000'
MYSQL_DB    = 'smellgood'

FLASK_PORT  = 5000      # HTTP API 端口（前端瀏覽器連線）
DEVICE_PORT = 9000      # TCP Socket 端口（BMduino 連線）

# ============================================================================
# Flask 應用程式
# ============================================================================
app = Flask(__name__, static_folder='static', static_url_path='')
CORS(app, resources={r"/api/*": {"origins": "*"}})

# ============================================================================
# 全域狀態
# ============================================================================
device_socket    = None          # 與 BMduino 的 TCP Socket 連線
device_lock      = threading.Lock()
device_connected = False

# task_id -> { status, result, subject_id, gender, age, bmi, meal_state, created_at }
pending_requests = {}      # 前端尚未被配對的請求
completed_tasks  = {}      # 已完成（或已配對）的任務結果


# ============================================================================
# 資料庫工具
# ============================================================================
def get_db():
    """取得 MySQL 連線"""
    return mysql.connector.connect(
        host=MYSQL_HOST,
        user=MYSQL_USER,
        password=MYSQL_PASS,
        database=MYSQL_DB
    )


# ============================================================================
# TCP Socket Server（背景執行緒：監聽 BMduino 連線）
# ============================================================================
def device_tcp_server():
    """在背景執行緒中執行，監聽 BMduino 的 TCP 連線"""
    global device_socket, device_connected

    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind(('0.0.0.0', DEVICE_PORT))
    server.listen(1)
    print(f"[Socket] 正在監聽 BMduino 連線，端口 {DEVICE_PORT}...")

    while True:
        try:
            client, addr = server.accept()
            print(f"[Socket] BMduino 已連線：{addr}")
            with device_lock:
                # 關閉舊連線（如果有）
                if device_socket:
                    try:
                        device_socket.close()
                    except Exception:
                        pass
                device_socket = client
                device_connected = True

            # 在當前執行緒處理此連線（阻塞式）
            handle_device_connection(client)

        except Exception as e:
            print(f"[Socket] Accept 錯誤: {e}")
            time.sleep(1)


def handle_device_connection(client):
    """處理來自 BMduino 的持續資料流"""
    global device_socket, device_connected

    buffer = ""
    client.settimeout(None)

    while True:
        try:
            data = client.recv(1024)
            if not data:
                break
            buffer += data.decode('utf-8', errors='ignore')

            # 以換行符號分割完整訊息
            while '\n' in buffer:
                line, buffer = buffer.split('\n', 1)
                line = line.strip()
                if line:
                    process_device_message(line, client)
        except ConnectionResetError:
            break
        except Exception as e:
            print(f"[Socket] 讀取錯誤: {e}")
            break

    with device_lock:
        device_socket = None
        device_connected = False
    print("[Socket] BMduino 已斷線")


def send_to_client(client, message):
    """透過 TCP Socket 傳送訊息給 BMduino"""
    try:
        client.sendall((message + '\n').encode('utf-8'))
        print(f"[→ Device] {message}")
        return True
    except Exception as e:
        print(f"[Socket] 傳送失敗: {e}")
        return False


def process_device_message(msg, client):
    """解析並處理來自 BMduino 的訊息（被動配對架構）"""
    print(f"[Device →] {msg}")
    parts = msg.split('|')

    if len(parts) < 1:
        return

    msg_type = parts[0]

    # ── HELLO 握手 ──
    if msg_type == 'HELLO':
        print(f"[Device] 裝置識別: {parts[1] if len(parts) > 1 else 'unknown'}")
        return

    # ── OFFER：BMduino 量測完成，詢問伺服器是否需要這筆資料 ──
    if msg_type == 'OFFER' and len(parts) >= 2:
        acetone_ppm = float(parts[1])
        handle_offer(client, acetone_ppm)
        return

    # ── RESULT：BMduino 完成預測，回傳結果 ──
    if msg_type == 'RESULT' and len(parts) >= 7:
        task_id       = parts[1]
        acetone_ppm   = float(parts[2])
        prediction    = int(parts[3])
        confidence    = float(parts[4])
        acetone_max   = float(parts[5])
        acetone_slope = float(parts[6])
        handle_prediction_result(task_id, acetone_ppm, prediction,
                                 confidence, acetone_max, acetone_slope)
        return


def handle_offer(client, acetone_ppm):
    """
    處理 BMduino 的 OFFER（量測完成通知）
    1. 尋找最近的待處理請求
    2. 有請求 → 檢查是否有前次紀錄 → ACCEPT 或 PREDICT
    3. 無請求 → REJECT
    """
    # 清除過期請求（超過 10 分鐘）
    now = time.time()
    expired = [k for k, v in pending_requests.items() if now - v['created_at'] > 600]
    for k in expired:
        completed_tasks[k] = {
            'status': 'timeout',
            'result': None,
            'created_at': pending_requests[k]['created_at']
        }
        del pending_requests[k]

    # 尋找最早的待處理請求
    if not pending_requests:
        print("[OFFER] 無待處理請求 → REJECT")
        send_to_client(client, "REJECT")
        return

    # 取最早的待處理請求
    task_id = min(pending_requests, key=lambda k: pending_requests[k]['created_at'])
    req = pending_requests.pop(task_id)

    subject_id = req['subject_id']
    meal_state = req['meal_state']
    gender     = req['gender']
    age        = req['age']
    bmi        = req['bmi']

    print(f"[OFFER] 配對到請求 {task_id}，受試者: {subject_id}")

    # 查詢是否有前次量測紀錄
    prev_record = None
    try:
        db = get_db()
        cursor = db.cursor(dictionary=True)
        cursor.execute(
            "SELECT acetone_ppm, uploaded_at, meal_state FROM `value` "
            "WHERE subject_id=%s ORDER BY uploaded_at DESC LIMIT 1",
            (subject_id,)
        )
        prev_record = cursor.fetchone()
        cursor.close()
        db.close()
    except Exception as e:
        print(f"[DB] 查詢失敗: {e}")

    if prev_record is not None:
        # ── 有前次紀錄 → PREDICT ──
        prev_acetone  = float(prev_record['acetone_ppm'])
        prev_time     = prev_record['uploaded_at']
        time_diff_min = (datetime.now() - prev_time).total_seconds() / 60.0
        gender_int    = 1 if gender == 'Male' else 0

        # 先寫入本次 value
        try:
            db = get_db()
            cursor = db.cursor()
            cursor.execute(
                "INSERT INTO `value` (subject_id, acetone_ppm, meal_state) "
                "VALUES (%s, %s, %s)",
                (subject_id, acetone_ppm, meal_state)
            )
            db.commit()
            cursor.close()
            db.close()
            print(f"[DB] 已插入 value: {subject_id}, {acetone_ppm:.3f} ppm")
        except Exception as e:
            print(f"[DB] value 插入失敗: {e}")

        # 傳送 PREDICT 給 BMduino
        cmd = (f"PREDICT|{task_id}|{meal_state}"
               f"|{prev_acetone:.3f}|{time_diff_min:.1f}"
               f"|{age}|{gender_int}|{bmi:.1f}")
        send_to_client(client, cmd)

        # 標記任務為等待預測
        completed_tasks[task_id] = {
            'status': 'predicting',
            'result': None,
            'device_status': 'predicting',
            'created_at': req['created_at'],
            'subject_id': subject_id,
            'acetone_ppm': acetone_ppm,
            'meal_state': meal_state
        }
        print(f"[OFFER] 有前次紀錄 → PREDICT 已發送")

    else:
        # ── 無前次紀錄 → ACCEPT（直接寫入） ──
        try:
            db = get_db()
            cursor = db.cursor()
            cursor.execute(
                "INSERT INTO `value` (subject_id, acetone_ppm, meal_state) "
                "VALUES (%s, %s, %s)",
                (subject_id, acetone_ppm, meal_state)
            )
            db.commit()
            cursor.close()
            db.close()
            print(f"[DB] 已插入 value: {subject_id}, {acetone_ppm:.3f} ppm")
        except Exception as e:
            print(f"[DB] value 插入失敗: {e}")

        send_to_client(client, "ACCEPT")

        # 直接標記任務完成（簡易結果）
        completed_tasks[task_id] = {
            'status': 'done',
            'result': {
                'type': 'simple',
                'acetone_ppm': acetone_ppm,
                'meal_state': meal_state
            },
            'created_at': req['created_at']
        }
        print(f"[OFFER] 首次量測 → ACCEPT，已寫入資料庫")


def handle_prediction_result(task_id, acetone_ppm, prediction,
                             confidence, acetone_max, acetone_slope):
    """處理 BMduino 回傳的預測結果"""
    if task_id not in completed_tasks:
        print(f"[RESULT] 找不到任務 {task_id}")
        return

    task = completed_tasks[task_id]
    subject_id = task.get('subject_id', '')
    meal_state = task.get('meal_state', '')

    # 寫入 ans 表
    try:
        db = get_db()
        cursor = db.cursor()
        cursor.execute(
            "INSERT INTO `ans` (subject_id, prediction, confidence, "
            "acetone_max, acetone_slope) VALUES (%s, %s, %s, %s, %s)",
            (subject_id, prediction, confidence, acetone_max, acetone_slope)
        )
        db.commit()
        cursor.close()
        db.close()
        print(f"[DB] 已插入 ans: {subject_id}, pred={prediction}, conf={confidence:.3f}")
    except Exception as e:
        print(f"[DB] ans 插入失敗: {e}")

    # 標記任務完成（詳細結果）
    task['status'] = 'done'
    task['result'] = {
        'type': 'detailed',
        'acetone_ppm': acetone_ppm,
        'meal_state': meal_state,
        'prediction': prediction,
        'confidence': confidence,
        'acetone_max': acetone_max,
        'acetone_slope': acetone_slope
    }
    print(f"[RESULT] 任務 {task_id} 已完成: {'Diabetes' if prediction else 'Healthy'}")


# ============================================================================
# Flask 路由
# ============================================================================

@app.route('/')
def index():
    """提供前端首頁"""
    return send_from_directory('static', 'index.html')


@app.route('/api/status')
def api_status():
    """查詢伺服器與裝置狀態"""
    return jsonify({
        'server': 'online',
        'device': 'connected' if device_connected else 'disconnected',
        'pendingCount': len(pending_requests)
    })


@app.route('/api/start', methods=['POST'])
def start_measurement():
    """
    前端提交受試者資料 → 建立待配對請求（不直接傳指令給 BMduino）
    等 BMduino 下次 OFFER 時自動配對
    """
    data = request.json

    subject_id = data.get('subjectId', '').strip()
    gender     = data.get('gender', 'Male')
    age        = int(data.get('age', 0))
    height     = float(data.get('height', 0))
    weight     = float(data.get('weight', 0))
    meal_state = data.get('mealState', 'fasting')

    # 驗證欄位
    if not subject_id or age <= 0 or height <= 0 or weight <= 0:
        return jsonify({'status': 'error', 'message': '請填寫所有必要欄位'}), 400

    bmi = round(weight / ((height / 100) ** 2), 2)

    # 寫入 / 更新 human 表
    try:
        db = get_db()
        cursor = db.cursor()
        cursor.execute("""
            INSERT INTO `human` (subject_id, gender, age, bmi)
            VALUES (%s, %s, %s, %s)
            ON DUPLICATE KEY UPDATE
                gender=VALUES(gender), age=VALUES(age), bmi=VALUES(bmi)
        """, (subject_id, gender, age, bmi))
        db.commit()
        cursor.close()
        db.close()
    except Exception as e:
        return jsonify({'status': 'error', 'message': f'資料庫錯誤: {str(e)}'}), 500

    # 建立待配對請求（不傳指令給 BMduino）
    task_id = str(uuid.uuid4())[:8]
    pending_requests[task_id] = {
        'subject_id': subject_id,
        'gender': gender,
        'age': age,
        'bmi': bmi,
        'meal_state': meal_state,
        'created_at': time.time()
    }

    print(f"[API] 待配對請求 {task_id} 已建立: {subject_id} ({meal_state})")
    print(f"[API] 當前待處理請求數: {len(pending_requests)}")

    return jsonify({
        'status': 'success',
        'taskId': task_id,
        'bmi': bmi,
        'message': '請求已建立，等待 BMduino 下次量測完成後自動配對'
    })


@app.route('/api/result/<task_id>')
def get_result(task_id):
    """前端輪詢量測結果"""
    # 先檢查已完成的任務
    if task_id in completed_tasks:
        task = completed_tasks[task_id]
        # 10 分鐘逾時
        if time.time() - task['created_at'] > 600:
            task['status'] = 'timeout'
        return jsonify({
            'status': task['status'],
            'deviceStatus': task.get('device_status', 'unknown'),
            'result': task.get('result')
        })

    # 檢查待配對的請求
    if task_id in pending_requests:
        return jsonify({
            'status': 'waiting',
            'deviceStatus': 'waiting_for_device',
            'result': None
        })

    return jsonify({'status': 'error', 'message': 'Task not found'}), 404


# ============================================================================
# 主程式
# ============================================================================
if __name__ == '__main__':
    # 啟動 TCP Socket Server（背景執行緒）
    tcp_thread = threading.Thread(target=device_tcp_server, daemon=True)
    tcp_thread.start()

    print("=" * 55)
    print("  E-Nose Full Stack Server (v2 被動配對)")
    print(f"  HTTP API:    http://0.0.0.0:{FLASK_PORT}")
    print(f"  Device TCP:  port {DEVICE_PORT}")
    print(f"  MySQL:       {MYSQL_USER}@{MYSQL_HOST}/{MYSQL_DB}")
    print("=" * 55)

    app.run(host='0.0.0.0', port=FLASK_PORT, debug=True, use_reloader=False)
