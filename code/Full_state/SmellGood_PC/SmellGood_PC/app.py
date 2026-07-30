from flask import Flask, request, jsonify
from flask_cors import CORS
import mysql.connector

app = Flask(__name__)
CORS(app, resources={r"/*": {"origins": "*"}})

def get_db_connection():
    return mysql.connector.connect(
        host="127.0.0.1",
        user="root",
        password="smellgood2026",
        database="smellgood"
    )

@app.route('/api/save', methods=['POST'])
def save_data():
    try:
        data = request.json
        # 從前端接收完整資料
        user_id = data.get('userId')
        gender = data.get('gender')
        age = data.get('age')
        bmi = data.get('bmi')
        acetone = data.get('acetone')
        slope = data.get('slope')
        state = data.get('state')
        
        db = get_db_connection()
        cursor = db.cursor()
        
        # 動作 1：寫入 human 表
        # 使用 ON DUPLICATE KEY UPDATE：如果同一個 ID 再吹一次，就更新他的年齡和 BMI
        query_human = """
            INSERT INTO `human` (ID, gender, age, BMI) 
            VALUES (%s, %s, %s, %s)
            ON DUPLICATE KEY UPDATE gender=VALUES(gender), age=VALUES(age), BMI=VALUES(BMI)
        """
        cursor.execute(query_human, (user_id, gender, age, bmi))
        
        # 動作 2：寫入 value 表 (只記錄 ID 和 濃度)
        cursor.execute("INSERT INTO `value` (ID, acetone_ppm) VALUES (%s, %s)", 
                       (user_id, acetone))
        
        # 動作 3：寫入 ans 表 (記錄 ID, 斜率, 狀態, 濃度)
        cursor.execute("INSERT INTO `ans` (ID, slope, state, acetone_ppm) VALUES (%s, %s, %s, %s)", 
                       (user_id, slope, state, acetone))
        
        # 確認送出所有動作
        db.commit()
        
        return jsonify({"status": "success", "message": "Records saved successfully!"}), 200
        
    except Exception as e:
        return jsonify({"status": "error", "message": str(e)}), 500
    finally:
        if 'db' in locals() and db.is_connected():
            cursor.close()
            db.close()

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True)